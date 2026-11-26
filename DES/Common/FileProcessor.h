// ============================================================================
//  FileProcessor.h
//  Шифрування / дешифрування файлу порціями (буфер FILE_BUFFER_SIZE) у
//  синхронному або асинхронному (Overlapped I/O) режимі.
//
//  Клас не залежить від алгоритму: дані обробляються через інтерфейс
//  IBufferProcessor, тому для ЛР4 (AES) він використовується без змін.
//
//  Доповнення останнього блоку (п. 7 завдання):
//   * шифрування: якщо розмір файлу не кратний розміру блоку B, останній
//     блок доповнюється байтами 0x00, а в кінець вихідного файлу дописується
//     1 байт — кількість "справжніх" байтів в останньому блоці (1..B-1).
//     Розмір зашифрованого файлу тоді k*B + 1 — НЕПАРНИЙ;
//   * дешифрування: непарна довжина файлу означає, що останній байт —
//     розмір останнього блоку; відповідно обрізаємо результат.
//     Парна довжина — доповнення не було, розмір має бути кратним B.
// ============================================================================
#pragma once

#include "Platform.h"
#include "BlockModes.h"
#include <string>

struct FileJob
{
    std::wstring inputPath;
    std::wstring outputPath;
    Direction    direction = Direction::Encrypt;
    bool         asyncIo   = true;      // true — асинхронна робота з файлами
};

struct FileJobResult
{
    bool         success        = false;
    std::wstring error;                 // опис помилки, якщо success == false
    uint64_t     inputSize      = 0;    // розмір вхідного файлу, байт
    uint64_t     outputSize     = 0;    // розмір вихідного файлу, байт
    unsigned     lastBlockBytes = 0;    // байтів в останньому неповному блоці (0 — доповнення не було)
    double       seconds        = 0.0;  // затрачений час
};

class FileProcessor
{
public:
    explicit FileProcessor(size_t bufferSize = FILE_BUFFER_SIZE);

    FileJobResult Run(const FileJob& job, IBufferProcessor& processor);

private:
    // План обробки файлу: що читати, скільки писати
    struct Plan
    {
        bool     encrypt    = true;
        size_t   blockSize  = 0;
        uint64_t inputSize  = 0;
        uint64_t dataSize   = 0;    // скільки байтів вхідного файлу є даними
                                    // (при дешифруванні без останнього службового байта)
        uint64_t outputSize = 0;
        unsigned tailBytes  = 0;    // розмір останнього неповного блоку (0 — немає)
        uint64_t chunkCount = 0;    // кількість порцій (буферів)
    };

    // Розміри однієї порції
    struct Chunk
    {
        uint64_t offset     = 0;    // зміщення у вхідному і вихідному файлах
        size_t   readLen    = 0;    // скільки байтів прочитати
        size_t   processLen = 0;    // скільки байтів шифрувати (кратно розміру блоку)
        size_t   writeLen   = 0;    // скільки байтів записати
        bool     isLast     = false;
    };

    bool  MakePlan(const FileJob& job, size_t blockSize, Plan& plan, std::wstring& error) const;
    Chunk GetChunk(const Plan& plan, uint64_t index) const;

    // Обробка порції в буфері: доповнення нулями -> шифрування -> службовий байт
    void ProcessChunk(const Plan& plan, const Chunk& chunk, uint8_t* buffer,
                      IBufferProcessor& processor) const;

    bool RunSync (const FileJob& job, const Plan& plan, IBufferProcessor& processor, std::wstring& error);
    bool RunAsync(const FileJob& job, const Plan& plan, IBufferProcessor& processor, std::wstring& error);

    static bool         IsSameFile(const std::wstring& a, const std::wstring& b);
    static std::wstring LastErrorText(const wchar_t* action);

    size_t m_bufferSize;
};
