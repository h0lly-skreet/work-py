// ============================================================================
//  DesApp.cpp
// ============================================================================
#include "DesApp.h"

#include "../Common/ConsoleUI.h"
#include "../Common/FileDialog.h"
#include "../Des/DesSelfTest.h"
#include "../Des/DesWeakKeys.h"

#include <cstdio>

// ----------------------------------------------------------------------------
int DesApp::Run()
{
    PrintHeader();

    if (!RunSelfTest())
    {
        ConsoleUI::WaitForKey(L"Натисніть будь-яку клавішу для виходу...");
        return 1;
    }

    // п. 9: після завершення пропонується повторити процес
    do
    {
        RunSession();
    } while (ConsoleUI::AskYesNo(L"Повторити процес шифрування/дешифрування?"));

    wprintf(L"\nРоботу програми завершено.\n");
    return 0;
}

void DesApp::PrintHeader() const
{
    wprintf(L"==============================================================================\n");
    wprintf(L"  Лабораторна робота №3. Алгоритми шифрування DES та 3DES\n");
    wprintf(L"  Виконав: Сайгаченко О. С., група КС-24-1\n");
    wprintf(L"==============================================================================\n");
    wprintf(L"  Програма шифрує/дешифрує файл алгоритмом DES або 3DES\n");
    wprintf(L"  (типи EEE3, EDE3, EEE2, EDE2) у режимах ECB або CBC.\n");
    wprintf(L"  Розмір буфера для роботи з файлом: %d КБ.\n", FILE_BUFFER_SIZE_KB);
    wprintf(L"==============================================================================\n");
}

bool DesApp::RunSelfTest() const
{
    wprintf(L"\nСамоперевірка реалізації DES (тест-вектори, порівняння оптимізованої\n"
            L"реалізації з еталонною на 10000 випадкових блоків, перевірка 3DES)... ");

    DesSelfTest test;
    std::wstring error;
    if (!test.Run(error))
    {
        wprintf(L"ПОМИЛКА!\n  %ls\n", error.c_str());
        return false;
    }
    wprintf(L"пройдено.\n");
    return true;
}

// ----------------------------------------------------------------------------
void DesApp::RunSession()
{
    Settings settings;

    if (!AskFiles(settings))
        return;

    AskAlgorithm(settings);
    AskMode(settings);
    AskKeys(settings);
    if (settings.mode == CipherMode::CBC)
        AskIv(settings);
    AskDirection(settings);
    AskIoMode(settings);

    PrintSummary(settings);
    Execute(settings);
}

// п. 1 — імена вхідного і вихідного файлів (GetOpenFileName / GetSaveFileName)
bool DesApp::AskFiles(Settings& settings) const
{
    wprintf(L"\n--- Крок 1. Вибір файлів ---\n");

    wprintf(L"Оберіть у діалоговому вікні ВХІДНИЙ файл (дані для обробки)...\n");
    if (!FileDialog::AskOpenFile(L"Оберіть вхідний файл", settings.inputPath))
    {
        wprintf(L"Вибір вхідного файлу скасовано.\n");
        return false;
    }
    wprintf(L"  Вхідний файл:  %ls\n", settings.inputPath.c_str());

    wprintf(L"Оберіть у діалоговому вікні ВИХІДНИЙ файл (куди записати результат)...\n");
    if (!FileDialog::AskSaveFile(L"Оберіть вихідний файл", settings.outputPath))
    {
        wprintf(L"Вибір вихідного файлу скасовано.\n");
        return false;
    }
    wprintf(L"  Вихідний файл: %ls\n", settings.outputPath.c_str());
    return true;
}

// п. 2 — алгоритм шифрування: DES або 3DES (з вибором одного з типів)
void DesApp::AskAlgorithm(Settings& settings) const
{
    wprintf(L"\n--- Крок 2. Алгоритм шифрування ---");

    const size_t algorithm = ConsoleUI::ReadChoice(L"Оберіть алгоритм:",
        { L"DES  (один 64-бітний ключ)",
          L"3DES (потрійний DES, 2 або 3 ключі)" });

    if (algorithm == 0)
    {
        settings.algorithm = Algorithm::Des;
        settings.keyCount  = 1;
        return;
    }

    settings.algorithm = Algorithm::TripleDes;

    const size_t type = ConsoleUI::ReadChoice(L"Оберіть тип 3DES:",
        { L"DES-EEE3: C = E_K3(E_K2(E_K1(T))), три різні ключі",
          L"DES-EDE3: C = E_K3(D_K2(E_K1(T))), три різні ключі",
          L"DES-EEE2: C = E_K1(E_K2(E_K1(T))), два ключі (K3 = K1)",
          L"DES-EDE2: C = E_K1(D_K2(E_K1(T))), два ключі (K3 = K1)" });

    static const TripleDesType TYPES[] = { TripleDesType::EEE3, TripleDesType::EDE3,
                                           TripleDesType::EEE2, TripleDesType::EDE2 };
    settings.tripleType = TYPES[type];
    settings.keyCount   = TripleDes::KeyCount(settings.tripleType);
}

// п. 3 — режим роботи алгоритму
void DesApp::AskMode(Settings& settings) const
{
    wprintf(L"\n--- Крок 3. Режим роботи алгоритму ---");

    const size_t mode = ConsoleUI::ReadChoice(L"Оберіть режим:",
        { L"ECB (Electronic Codebook) — кожен блок шифрується незалежно",
          L"CBC (Cipher Block Chaining) — зчеплення блоків, потрібен вектор IV" });

    settings.mode = (mode == 0) ? CipherMode::ECB : CipherMode::CBC;
}

// п. 4 — введення ключа (ключів) і перевірка на слабкість
void DesApp::AskKeys(Settings& settings) const
{
    wprintf(L"\n--- Крок 4. Введення ключ%ls ---\n", settings.keyCount == 1 ? L"а" : L"ів");
    wprintf(L"Ключ — 64 біти (8 байт). Кожна пара hex-цифр задає черговий байт ключа.\n"
            L"Біти парності (останній біт кожного байта) алгоритмом не використовуються.\n");

    for (unsigned i = 0; i < settings.keyCount; ++i)
    {
        const std::wstring name = (settings.keyCount == 1) ? L"ключ K"
                                                           : L"ключ K" + std::to_wstring(i + 1);
        settings.keys[i] = AskKey(name);
    }
}

uint64_t DesApp::AskKey(const std::wstring& keyName) const
{
    for (;;)
    {
        const std::vector<uint8_t> bytes = ConsoleUI::ReadHexBytes(L"Введіть " + keyName + L":", 8);

        // 1-й введений байт — старший байт 64-бітного ключа
        uint64_t key = 0;
        for (uint8_t b : bytes)
            key = (key << 8) | b;

        uint64_t pairKey = 0;
        switch (DesWeakKeys::Check(key, &pairKey))
        {
        case DesKeyStrength::Weak:
            wprintf(L"  УВАГА: %ls є СЛАБКИМ ключем DES (табл. 3.14):\n"
                    L"  повторне шифрування цим ключем відновлює вхідний текст.\n"
                    L"  Введіть інший ключ.\n", ConsoleUI::ToHex64(key).c_str());
            break;

        case DesKeyStrength::SemiWeak:
            wprintf(L"  УВАГА: %ls є ЧАСТКОВО СЛАБКИМ ключем (табл. 3.15),\n"
                    L"  утворює пару з ключем %ls: DES(DES(T, K1), K2) = T.\n"
                    L"  Введіть інший ключ.\n",
                    ConsoleUI::ToHex64(key).c_str(), ConsoleUI::ToHex64(pairKey).c_str());
            break;

        default:
            wprintf(L"  Прийнято %ls = %ls (не слабкий і не частково слабкий).\n",
                    keyName.c_str(), ConsoleUI::ToHex64(key).c_str());
            return key;
        }
    }
}

// п. 5 — початковий вектор IV (лише для CBC)
void DesApp::AskIv(Settings& settings) const
{
    wprintf(L"\n--- Крок 5. Початковий вектор IV ---\n");
    wprintf(L"IV — 64 біти (8 байт), використовується для зчеплення 1-го блоку в режимі CBC.\n");

    const std::vector<uint8_t> bytes = ConsoleUI::ReadHexBytes(L"Введіть вектор IV:", 8);
    for (size_t i = 0; i < 8; ++i)
        settings.iv[i] = bytes[i];

    wprintf(L"  Прийнято IV = %ls\n", ConsoleUI::BytesToHex(settings.iv, 8).c_str());
}

// п. 6 — шифрування чи дешифрування
void DesApp::AskDirection(Settings& settings) const
{
    wprintf(L"\n--- Крок 6. Режим роботи ---");

    const size_t direction = ConsoleUI::ReadChoice(L"Що потрібно зробити з файлом?",
        { L"Шифрування", L"Дешифрування" });

    settings.direction = (direction == 0) ? Direction::Encrypt : Direction::Decrypt;
}

void DesApp::AskIoMode(Settings& settings) const
{
    wprintf(L"\n--- Крок 7. Робота з файлами ---");

    const size_t io = ConsoleUI::ReadChoice(L"Оберіть спосіб роботи з файлами:",
        { L"Асинхронний (Overlapped I/O: читання, шифрування і запис виконуються паралельно)",
          L"Синхронний (читання -> шифрування -> запис по черзі)" });

    settings.asyncIo = (io == 0);
}

// ----------------------------------------------------------------------------
std::wstring DesApp::AlgorithmName(const Settings& settings)
{
    if (settings.algorithm == Algorithm::Des)
        return L"DES";

    switch (settings.tripleType)
    {
    case TripleDesType::EEE3: return L"3DES (DES-EEE3)";
    case TripleDesType::EDE3: return L"3DES (DES-EDE3)";
    case TripleDesType::EEE2: return L"3DES (DES-EEE2)";
    default:                  return L"3DES (DES-EDE2)";
    }
}

std::wstring DesApp::FormatSize(uint64_t bytes)
{
    wchar_t text[64];
    if (bytes >= 1024ull * 1024)
        swprintf(text, 64, L"%llu байт (%.2f МБ)", static_cast<unsigned long long>(bytes),
                 bytes / (1024.0 * 1024.0));
    else
        swprintf(text, 64, L"%llu байт", static_cast<unsigned long long>(bytes));
    return text;
}

void DesApp::PrintSummary(const Settings& settings) const
{
    wprintf(L"\n------------------------------ Параметри ------------------------------\n");
    wprintf(L"  Вхідний файл:   %ls\n", settings.inputPath.c_str());
    wprintf(L"  Вихідний файл:  %ls\n", settings.outputPath.c_str());
    wprintf(L"  Алгоритм:       %ls\n", AlgorithmName(settings).c_str());
    wprintf(L"  Режим:          %ls\n", settings.mode == CipherMode::ECB ? L"ECB" : L"CBC");
    for (unsigned i = 0; i < settings.keyCount; ++i)
        wprintf(L"  Ключ K%u:        %ls\n", i + 1, ConsoleUI::ToHex64(settings.keys[i]).c_str());
    if (settings.mode == CipherMode::CBC)
        wprintf(L"  Вектор IV:      %ls\n", ConsoleUI::BytesToHex(settings.iv, 8).c_str());
    wprintf(L"  Дія:            %ls\n", settings.direction == Direction::Encrypt ? L"шифрування" : L"дешифрування");
    wprintf(L"  Робота з файлами: %ls\n", settings.asyncIo ? L"асинхронна" : L"синхронна");
    wprintf(L"-----------------------------------------------------------------------\n");
}

std::unique_ptr<IBufferProcessor> DesApp::CreateProcessor(const Settings& settings) const
{
    const uint8_t* iv = (settings.mode == CipherMode::CBC) ? settings.iv : nullptr;

    if (settings.algorithm == Algorithm::Des)
    {
        const Des cipher(settings.keys[0]);
        return std::make_unique<ModeProcessor<Des>>(cipher, settings.mode, settings.direction, iv);
    }

    const TripleDes cipher(settings.tripleType, settings.keys[0], settings.keys[1], settings.keys[2]);
    return std::make_unique<ModeProcessor<TripleDes>>(cipher, settings.mode, settings.direction, iv);
}

// п. 7-8 — обробка файлу і виведення затраченого часу
void DesApp::Execute(const Settings& settings)
{
    const bool encrypt = (settings.direction == Direction::Encrypt);
    wprintf(L"\nВиконується %ls файлу, зачекайте...\n", encrypt ? L"шифрування" : L"дешифрування");

    // Ключі раундів генеруються тут ОДИН раз для всього файлу
    std::unique_ptr<IBufferProcessor> processor = CreateProcessor(settings);

    FileJob job;
    job.inputPath  = settings.inputPath;
    job.outputPath = settings.outputPath;
    job.direction  = settings.direction;
    job.asyncIo    = settings.asyncIo;

    const FileJobResult result = m_fileProcessor.Run(job, *processor);

    if (!result.success)
    {
        wprintf(L"\nПОМИЛКА: %ls\n", result.error.c_str());
        return;
    }

    wprintf(L"\n%ls успішно завершено.\n", encrypt ? L"Шифрування" : L"Дешифрування");
    wprintf(L"  Розмір вхідного файлу:  %ls\n", FormatSize(result.inputSize).c_str());
    wprintf(L"  Розмір вихідного файлу: %ls\n", FormatSize(result.outputSize).c_str());

    if (result.lastBlockBytes != 0)
    {
        if (encrypt)
            wprintf(L"  Розмір файлу не кратний 8 байтам: останній блок (%u байт) доповнено\n"
                    L"  нулями до 8 байт, у кінець файлу дописано 1 службовий байт = %u.\n",
                    result.lastBlockBytes, result.lastBlockBytes);
        else
            wprintf(L"  Непарна довжина файлу: службовий байт = %u, останній блок\n"
                    L"  обрізано до %u байт.\n", result.lastBlockBytes, result.lastBlockBytes);
    }

    wprintf(L"  Затрачений час: %.3f с", result.seconds);
    if (result.seconds > 0.0 && result.inputSize > 0)
        wprintf(L"  (швидкість %.2f МБ/с)", result.inputSize / (1024.0 * 1024.0) / result.seconds);
    wprintf(L"\n");
}
