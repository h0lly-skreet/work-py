// ============================================================================
//  DesApp.h
//  Консольний додаток лабораторної роботи №3: діалог з користувачем
//  (пункти 1-9 завдання) і запуск шифрування/дешифрування файлу.
// ============================================================================
#pragma once

#include "../Common/BlockModes.h"
#include "../Common/FileProcessor.h"
#include "../Des/Des.h"

#include <memory>
#include <string>

class DesApp
{
public:
    int Run();

private:
    enum class Algorithm
    {
        Des,
        TripleDes
    };

    // Усі параметри одного сеансу шифрування/дешифрування
    struct Settings
    {
        std::wstring  inputPath;
        std::wstring  outputPath;
        Algorithm     algorithm   = Algorithm::Des;
        TripleDesType tripleType  = TripleDesType::EDE3;
        CipherMode    mode        = CipherMode::ECB;
        unsigned      keyCount    = 1;
        uint64_t      keys[3]     = { 0, 0, 0 };
        uint8_t       iv[8]       = {};
        Direction     direction   = Direction::Encrypt;
        bool          asyncIo     = true;
    };

    void PrintHeader() const;
    bool RunSelfTest() const;

    // Один повний сеанс (пункти 1-8)
    void RunSession();

    bool AskFiles(Settings& settings) const;        // п. 1
    void AskAlgorithm(Settings& settings) const;    // п. 2
    void AskMode(Settings& settings) const;         // п. 3
    void AskKeys(Settings& settings) const;         // п. 4
    void AskIv(Settings& settings) const;           // п. 5
    void AskDirection(Settings& settings) const;    // п. 6
    void AskIoMode(Settings& settings) const;       // режим роботи з файлами

    // Введення одного ключа з перевіркою на слабкість (повторне введення, якщо слабкий)
    uint64_t AskKey(const std::wstring& keyName) const;

    void PrintSummary(const Settings& settings) const;
    void Execute(const Settings& settings);         // п. 7-8

    std::unique_ptr<IBufferProcessor> CreateProcessor(const Settings& settings) const;

    static std::wstring AlgorithmName(const Settings& settings);
    static std::wstring FormatSize(uint64_t bytes);

    FileProcessor m_fileProcessor;
};
