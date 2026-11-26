// ============================================================================
//  FileProcessor.cpp
// ============================================================================
#include "FileProcessor.h"
#include "Timer.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

// ----------------------------------------------------------------------------
//  Допоміжні класи
// ----------------------------------------------------------------------------
namespace
{
    // Дескриптор файлу, який закривається автоматично (RAII)
    class FileHandle
    {
    public:
        explicit FileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
        ~FileHandle() { Close(); }
        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;

        HANDLE Get() const   { return m_handle; }
        bool   IsValid() const { return m_handle != INVALID_HANDLE_VALUE; }
        void   Close()
        {
            if (IsValid())
            {
                CloseHandle(m_handle);
                m_handle = INVALID_HANDLE_VALUE;
            }
        }

    private:
        HANDLE m_handle;
    };

    // Буфер, вирівняний на межу сторінки пам'яті (VirtualAlloc) —
    // найкращий варіант для операцій введення/виведення
    class IoBuffer
    {
    public:
        explicit IoBuffer(size_t size)
            : m_data(static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))) {}
        ~IoBuffer() { if (m_data) VirtualFree(m_data, 0, MEM_RELEASE); }
        IoBuffer(const IoBuffer&) = delete;
        IoBuffer& operator=(const IoBuffer&) = delete;

        uint8_t* Data() const { return m_data; }

    private:
        uint8_t* m_data;
    };

    inline uint64_t RoundUp(uint64_t value, uint64_t multiple)
    {
        return (value + multiple - 1) / multiple * multiple;
    }
}

// ----------------------------------------------------------------------------
FileProcessor::FileProcessor(size_t bufferSize)
    : m_bufferSize(bufferSize)
{
}

std::wstring FileProcessor::LastErrorText(const wchar_t* action)
{
    const DWORD code = GetLastError();
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring text = std::wstring(action) + L" (код помилки " + std::to_wstring(code) + L")";
    if (message != nullptr)
    {
        text += L": ";
        text += message;
        LocalFree(message);
        while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r'))
            text.pop_back();
    }
    return text;
}

bool FileProcessor::IsSameFile(const std::wstring& a, const std::wstring& b)
{
    wchar_t fullA[MAX_PATH * 4], fullB[MAX_PATH * 4];
    if (!GetFullPathNameW(a.c_str(), MAX_PATH * 4, fullA, nullptr) ||
        !GetFullPathNameW(b.c_str(), MAX_PATH * 4, fullB, nullptr))
        return false;
    return CompareStringOrdinal(fullA, -1, fullB, -1, TRUE) == CSTR_EQUAL;
}

// ----------------------------------------------------------------------------
//  План обробки
// ----------------------------------------------------------------------------
bool FileProcessor::MakePlan(const FileJob& job, size_t blockSize, Plan& plan, std::wstring& error) const
{
    FileHandle input(CreateFileW(job.inputPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!input.IsValid())
    {
        error = LastErrorText(L"Не вдалося відкрити вхідний файл");
        return false;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(input.Get(), &size))
    {
        error = LastErrorText(L"Не вдалося визначити розмір вхідного файлу");
        return false;
    }

    plan.encrypt   = (job.direction == Direction::Encrypt);
    plan.blockSize = blockSize;
    plan.inputSize = static_cast<uint64_t>(size.QuadPart);

    if (plan.encrypt)
    {
        plan.dataSize   = plan.inputSize;
        plan.tailBytes  = static_cast<unsigned>(plan.inputSize % blockSize);
        plan.outputSize = RoundUp(plan.inputSize, blockSize) + (plan.tailBytes != 0 ? 1 : 0);
    }
    else if (plan.inputSize % 2 == 1)
    {
        // Непарна довжина: останній байт — розмір останнього блоку
        plan.dataSize = plan.inputSize - 1;

        LARGE_INTEGER position;
        position.QuadPart = static_cast<LONGLONG>(plan.dataSize);
        uint8_t tail = 0;
        DWORD read = 0;
        if (!SetFilePointerEx(input.Get(), position, nullptr, FILE_BEGIN) ||
            !ReadFile(input.Get(), &tail, 1, &read, nullptr) || read != 1)
        {
            error = LastErrorText(L"Не вдалося прочитати службовий байт у кінці файлу");
            return false;
        }

        if (plan.dataSize % blockSize != 0 || plan.dataSize < blockSize ||
            tail == 0 || tail >= blockSize)
        {
            error = L"Файл не є коректним результатом шифрування: неправильна довжина "
                    L"або службовий байт розміру останнього блоку.";
            return false;
        }

        plan.tailBytes  = tail;
        plan.outputSize = plan.dataSize - blockSize + tail;
    }
    else
    {
        // Парна довжина: доповнення не було
        if (plan.inputSize % blockSize != 0)
        {
            error = L"Файл не є коректним результатом шифрування: довжина файлу парна, "
                    L"але не кратна розміру блоку (" + std::to_wstring(blockSize) + L" байт).";
            return false;
        }
        plan.dataSize   = plan.inputSize;
        plan.tailBytes  = 0;
        plan.outputSize = plan.inputSize;
    }

    plan.chunkCount = (plan.dataSize + m_bufferSize - 1) / m_bufferSize;
    return true;
}

FileProcessor::Chunk FileProcessor::GetChunk(const Plan& plan, uint64_t index) const
{
    Chunk chunk;
    chunk.offset  = index * m_bufferSize;
    chunk.readLen = static_cast<size_t>(std::min<uint64_t>(m_bufferSize, plan.dataSize - chunk.offset));
    chunk.isLast  = (index + 1 == plan.chunkCount);

    if (plan.encrypt)
    {
        chunk.processLen = static_cast<size_t>(RoundUp(chunk.readLen, plan.blockSize));
        chunk.writeLen   = chunk.processLen + ((chunk.isLast && plan.tailBytes != 0) ? 1 : 0);
    }
    else
    {
        chunk.processLen = chunk.readLen;
        chunk.writeLen   = (chunk.isLast && plan.tailBytes != 0)
                         ? chunk.readLen - plan.blockSize + plan.tailBytes
                         : chunk.readLen;
    }
    return chunk;
}

void FileProcessor::ProcessChunk(const Plan& plan, const Chunk& chunk, uint8_t* buffer,
                                 IBufferProcessor& processor) const
{
    // Доповнення останнього блоку байтами 0x00
    if (chunk.processLen > chunk.readLen)
        std::memset(buffer + chunk.readLen, 0, chunk.processLen - chunk.readLen);

    processor.Process(buffer, chunk.processLen);

    // Службовий байт — справжній розмір останнього блоку
    if (plan.encrypt && chunk.isLast && plan.tailBytes != 0)
        buffer[chunk.processLen] = static_cast<uint8_t>(plan.tailBytes);
}

// ----------------------------------------------------------------------------
//  Головний метод
// ----------------------------------------------------------------------------
FileJobResult FileProcessor::Run(const FileJob& job, IBufferProcessor& processor)
{
    FileJobResult result;

    if (IsSameFile(job.inputPath, job.outputPath))
    {
        result.error = L"Вхідний і вихідний файли збігаються. Оберіть інший вихідний файл.";
        return result;
    }

    Timer timer;            // вимірюється весь процес: відкриття, обробка, запис, закриття

    Plan plan;
    if (!MakePlan(job, processor.BlockSize(), plan, result.error))
        return result;

    const bool ok = job.asyncIo ? RunAsync(job, plan, processor, result.error)
                                : RunSync (job, plan, processor, result.error);

    result.seconds = timer.ElapsedSeconds();

    if (!ok)
    {
        DeleteFileW(job.outputPath.c_str());    // не залишаємо пошкоджений результат
        return result;
    }

    result.success        = true;
    result.inputSize      = plan.inputSize;
    result.outputSize     = plan.outputSize;
    result.lastBlockBytes = plan.tailBytes;
    return result;
}

// ----------------------------------------------------------------------------
//  СИНХРОННИЙ режим: прочитати порцію -> обробити -> записати -> наступна.
//  Поки йде читання/запис, процесор простоює, і навпаки.
// ----------------------------------------------------------------------------
bool FileProcessor::RunSync(const FileJob& job, const Plan& plan, IBufferProcessor& processor,
                            std::wstring& error)
{
    FileHandle input(CreateFileW(job.inputPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!input.IsValid())
    {
        error = LastErrorText(L"Не вдалося відкрити вхідний файл");
        return false;
    }

    FileHandle output(CreateFileW(job.outputPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!output.IsValid())
    {
        error = LastErrorText(L"Не вдалося створити вихідний файл");
        return false;
    }

    IoBuffer buffer(m_bufferSize + plan.blockSize);     // + запас на доповнення і службовий байт
    if (buffer.Data() == nullptr)
    {
        error = L"Недостатньо пам'яті для буфера.";
        return false;
    }

    for (uint64_t index = 0; index < plan.chunkCount; ++index)
    {
        const Chunk chunk = GetChunk(plan, index);

        DWORD done = 0;
        if (!ReadFile(input.Get(), buffer.Data(), static_cast<DWORD>(chunk.readLen), &done, nullptr) ||
            done != chunk.readLen)
        {
            error = LastErrorText(L"Помилка читання вхідного файлу");
            return false;
        }

        ProcessChunk(plan, chunk, buffer.Data(), processor);

        if (!WriteFile(output.Get(), buffer.Data(), static_cast<DWORD>(chunk.writeLen), &done, nullptr) ||
            done != chunk.writeLen)
        {
            error = LastErrorText(L"Помилка запису вихідного файлу");
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
//  АСИНХРОННИЙ режим (Overlapped I/O).
//
//  Файли відкриваються з прапорцем FILE_FLAG_OVERLAPPED: ReadFile / WriteFile
//  лише ЗАПУСКАЮТЬ операцію і одразу повертають керування, а завершення
//  очікується пізніше через GetOverlappedResult.
//
//  Використовується конвеєр з ASYNC_BUFFER_COUNT (3) буферів. Поки процесор
//  шифрує порцію i, диск паралельно ЧИТАЄ порцію i+1 і ЗАПИСУЄ порцію i-1:
//
//      час --->
//      буфер 0: [читання 0][шифр. 0][запис 0 ...........][читання 3]...
//      буфер 1:            [читання 1][шифр. 1][запис 1 ..........]...
//      буфер 2:                       [читання 2][шифр. 2][запис 2]...
// ----------------------------------------------------------------------------
namespace
{
    // Один буфер конвеєра з власними структурами OVERLAPPED для читання і запису
    struct AsyncSlot
    {
        explicit AsyncSlot(size_t size) : buffer(size)
        {
            readOv.hEvent  = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            writeOv.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        ~AsyncSlot()
        {
            if (readOv.hEvent)  CloseHandle(readOv.hEvent);
            if (writeOv.hEvent) CloseHandle(writeOv.hEvent);
        }
        AsyncSlot(const AsyncSlot&) = delete;
        AsyncSlot& operator=(const AsyncSlot&) = delete;

        bool IsValid() const { return buffer.Data() && readOv.hEvent && writeOv.hEvent; }

        IoBuffer   buffer;
        OVERLAPPED readOv  = {};
        OVERLAPPED writeOv = {};
        bool       readPending  = false;
        bool       writePending = false;
        DWORD      expectedRead  = 0;
        DWORD      expectedWrite = 0;
    };

    // Запуск асинхронної операції (читання або запису) зі зміщенням offset
    bool StartIo(HANDLE file, bool isWrite, uint8_t* data, DWORD length, uint64_t offset, OVERLAPPED& ov)
    {
        ov.Offset     = static_cast<DWORD>(offset);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
        ResetEvent(ov.hEvent);

        const BOOL started = isWrite ? WriteFile(file, data, length, nullptr, &ov)
                                     : ReadFile (file, data, length, nullptr, &ov);
        // FALSE + ERROR_IO_PENDING — нормальна ситуація: операція виконується у фоні
        return started || GetLastError() == ERROR_IO_PENDING;
    }

    // Очікування завершення операції
    bool WaitIo(HANDLE file, OVERLAPPED& ov, DWORD expected)
    {
        DWORD done = 0;
        return GetOverlappedResult(file, &ov, &done, TRUE) && done == expected;
    }
}

bool FileProcessor::RunAsync(const FileJob& job, const Plan& plan, IBufferProcessor& processor,
                             std::wstring& error)
{
    FileHandle input(CreateFileW(job.inputPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!input.IsValid())
    {
        error = LastErrorText(L"Не вдалося відкрити вхідний файл");
        return false;
    }

    FileHandle output(CreateFileW(job.outputPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, nullptr));
    if (!output.IsValid())
    {
        error = LastErrorText(L"Не вдалося створити вихідний файл");
        return false;
    }

    // Одразу задаємо остаточний розмір вихідного файлу: місце на диску
    // виділяється один раз, а не нарощується з кожним записом
    LARGE_INTEGER finalSize;
    finalSize.QuadPart = static_cast<LONGLONG>(plan.outputSize);
    if (!SetFilePointerEx(output.Get(), finalSize, nullptr, FILE_BEGIN) || !SetEndOfFile(output.Get()))
    {
        error = LastErrorText(L"Не вдалося виділити місце для вихідного файлу");
        return false;
    }

    std::vector<std::unique_ptr<AsyncSlot>> slots;
    for (int i = 0; i < ASYNC_BUFFER_COUNT; ++i)
        slots.push_back(std::make_unique<AsyncSlot>(m_bufferSize + plan.blockSize));

    // Завершення: дочекатися всіх незавершених операцій і звільнити буфери.
    // При помилці незавершені операції скасовуються (CancelIoEx), бо буфер
    // не можна звільняти, поки система ще пише в нього або читає з нього.
    auto finish = [&](bool cancel) -> bool
    {
        bool ok = true;
        for (auto& slot : slots)
        {
            if (slot->readPending)
            {
                if (cancel) CancelIoEx(input.Get(), &slot->readOv);
                ok = WaitIo(input.Get(), slot->readOv, slot->expectedRead) && ok;
                slot->readPending = false;
            }
            if (slot->writePending)
            {
                if (cancel) CancelIoEx(output.Get(), &slot->writeOv);
                ok = WaitIo(output.Get(), slot->writeOv, slot->expectedWrite) && ok;
                slot->writePending = false;
            }
        }
        slots.clear();          // звільнення буферів
        return ok;
    };

    auto fail = [&](const wchar_t* action) -> bool
    {
        error = LastErrorText(action);
        finish(true);
        return false;
    };

    for (auto& slot : slots)
    {
        if (!slot->IsValid())
        {
            error = L"Недостатньо пам'яті або ресурсів для асинхронних буферів.";
            finish(true);
            return false;
        }
    }

    // Запуск читання порції index у відповідний буфер
    auto startRead = [&](uint64_t index) -> bool
    {
        AsyncSlot& slot = *slots[index % ASYNC_BUFFER_COUNT];

        // Буфер ще може записуватися (порція index - 3) — чекаємо, поки звільниться
        if (slot.writePending)
        {
            slot.writePending = false;
            if (!WaitIo(output.Get(), slot.writeOv, slot.expectedWrite))
                return false;
        }

        const Chunk chunk = GetChunk(plan, index);
        slot.expectedRead = static_cast<DWORD>(chunk.readLen);
        if (!StartIo(input.Get(), false, slot.buffer.Data(), slot.expectedRead, chunk.offset, slot.readOv))
            return false;
        slot.readPending = true;
        return true;
    };

    if (plan.chunkCount > 0 && !startRead(0))
        return fail(L"Помилка читання вхідного файлу");

    for (uint64_t index = 0; index < plan.chunkCount; ++index)
    {
        AsyncSlot& slot = *slots[index % ASYNC_BUFFER_COUNT];
        const Chunk chunk = GetChunk(plan, index);

        // 1. Чекаємо, поки порція index дочитається
        slot.readPending = false;
        if (!WaitIo(input.Get(), slot.readOv, slot.expectedRead))
            return fail(L"Помилка читання вхідного файлу");

        // 2. Одразу запускаємо читання НАСТУПНОЇ порції — воно йде паралельно з шифруванням
        if (index + 1 < plan.chunkCount && !startRead(index + 1))
            return fail(L"Помилка читання вхідного файлу");

        // 3. Шифруємо / дешифруємо поточну порцію
        ProcessChunk(plan, chunk, slot.buffer.Data(), processor);

        // 4. Запускаємо запис і, не чекаючи його завершення, переходимо далі
        slot.expectedWrite = static_cast<DWORD>(chunk.writeLen);
        if (!StartIo(output.Get(), true, slot.buffer.Data(), slot.expectedWrite, chunk.offset, slot.writeOv))
            return fail(L"Помилка запису вихідного файлу");
        slot.writePending = true;
    }

    // Дочікуємося завершення останніх записів
    if (!finish(false))
    {
        error = LastErrorText(L"Помилка запису вихідного файлу");
        return false;
    }
    return true;
}
