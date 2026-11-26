// ============================================================================
//  ConsoleUI.h
//  Введення даних у консолі з контролем символів:
//    * вибір пункту меню;
//    * введення ключа / IV у шістнадцятковому вигляді.
//
//  В обох випадках: можна вводити ЛИШЕ допустимі символи (інші натискання
//  ігноруються), Backspace видаляє останній символ (редагування), Enter
//  завершує введення ЛИШЕ коли введено потрібну кількість символів.
//  Підхід такий самий, як у прикладі викладача EnterPassword.cpp.
// ============================================================================
#pragma once

#include "Platform.h"
#include <string>
#include <vector>

class ConsoleUI
{
public:
    // Налаштування консолі на вивід Unicode (кирилиця) та заголовок вікна
    static void Initialize(const std::wstring& windowTitle);

    // Виводить заголовок і пронумеровані варіанти (1..N, N <= 9), повертає
    // індекс обраного варіанту (0..N-1)
    static size_t ReadChoice(const std::wstring& title, const std::vector<std::wstring>& options);

    // Питання "так/ні"
    static bool AskYesNo(const std::wstring& question);

    // Введення byteCount байтів у вигляді 2*byteCount шістнадцяткових цифр;
    // кожна пара цифр — черговий байт (1-ша пара — 1-й байт)
    static std::vector<uint8_t> ReadHexBytes(const std::wstring& prompt, size_t byteCount);

    // Байти -> рядок hex ("5B5A5767...")
    static std::wstring BytesToHex(const uint8_t* data, size_t length);

    // 64-бітне число -> рядок з 16 hex-цифр
    static std::wstring ToHex64(uint64_t value);

    // Чекає натискання будь-якої клавіші
    static void WaitForKey(const std::wstring& message);

private:
    static constexpr wchar_t KEY_BACKSPACE = 8;
    static constexpr wchar_t KEY_ENTER     = 13;

    // Зчитує натискання клавіші без відлуння. Спеціальні клавіші (стрілки,
    // F1..F12 тощо) приходять двома кодами: 0 або 0xE0 і код клавіші —
    // такі натискання повністю відкидаються (повертається 0), інакше,
    // наприклад, F7 сприймався б як символ 'A'.
    static wchar_t ReadKey();

    // Видалення останнього виведеного символу з екрана
    static void EraseChar();
};
