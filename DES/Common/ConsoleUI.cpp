// ============================================================================
//  ConsoleUI.cpp
// ============================================================================
#include "ConsoleUI.h"

#include <windows.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <cwctype>

void ConsoleUI::Initialize(const std::wstring& windowTitle)
{
    // Після цього весь вивід — лише через широкі функції (wprintf, putwchar)
    _setmode(_fileno(stdout), _O_U16TEXT);
    SetConsoleTitleW(windowTitle.c_str());
}

wchar_t ConsoleUI::ReadKey()
{
    const wint_t key = _getwch();
    if (key == 0 || key == 0xE0)
    {
        _getwch();          // другий код спеціальної клавіші — відкидаємо
        return 0;
    }
    return static_cast<wchar_t>(key);
}

void ConsoleUI::EraseChar()
{
    wprintf(L"\b \b");
}

size_t ConsoleUI::ReadChoice(const std::wstring& title, const std::vector<std::wstring>& options)
{
    wprintf(L"\n%ls\n", title.c_str());
    for (size_t i = 0; i < options.size(); ++i)
        wprintf(L"  %zu - %ls\n", i + 1, options[i].c_str());
    wprintf(L"Введіть номер (1-%zu) і натисніть Enter: ", options.size());

    wchar_t entered = 0;            // 0 — ще нічого не введено
    for (;;)
    {
        const wchar_t key = ReadKey();

        if (key >= L'1' && key < static_cast<wchar_t>(L'1' + options.size()))
        {
            if (entered == 0)       // приймаємо лише одну цифру
            {
                entered = key;
                putwchar(key);
            }
        }
        else if (key == KEY_BACKSPACE && entered != 0)
        {
            EraseChar();
            entered = 0;
        }
        else if (key == KEY_ENTER && entered != 0)
        {
            break;
        }
        // будь-які інші символи ігноруються
    }

    const size_t index = static_cast<size_t>(entered - L'1');
    wprintf(L"\n  Обрано: %ls\n", options[index].c_str());
    return index;
}

bool ConsoleUI::AskYesNo(const std::wstring& question)
{
    return ReadChoice(question, { L"Так", L"Ні" }) == 0;
}

std::vector<uint8_t> ConsoleUI::ReadHexBytes(const std::wstring& prompt, size_t byteCount)
{
    const size_t digitCount = byteCount * 2;
    std::vector<uint8_t> digits;            // введені цифри (значення 0..15)
    digits.reserve(digitCount);

    wprintf(L"\n%ls\n", prompt.c_str());
    wprintf(L"  (%zu шістнадцяткових цифр 0-9, A-F; Backspace — видалити цифру; "
            L"Enter — завершити після введення всіх %zu цифр)\n  > ", digitCount, digitCount);

    for (;;)
    {
        const wchar_t key = static_cast<wchar_t>(std::towupper(ReadKey()));

        const bool isDigit  = (key >= L'0' && key <= L'9');
        const bool isLetter = (key >= L'A' && key <= L'F');

        if (isDigit || isLetter)
        {
            if (digits.size() < digitCount)
            {
                putwchar(key);
                digits.push_back(static_cast<uint8_t>(isDigit ? key - L'0' : key - L'A' + 10));
            }
        }
        else if (key == KEY_BACKSPACE && !digits.empty())
        {
            EraseChar();
            digits.pop_back();
        }
        else if (key == KEY_ENTER && digits.size() == digitCount)
        {
            break;
        }
    }
    wprintf(L"\n");

    // Кожна пара цифр — черговий байт: старша тетрада, молодша тетрада
    std::vector<uint8_t> bytes(byteCount);
    for (size_t i = 0; i < byteCount; ++i)
        bytes[i] = static_cast<uint8_t>((digits[2 * i] << 4) | digits[2 * i + 1]);
    return bytes;
}

std::wstring ConsoleUI::BytesToHex(const uint8_t* data, size_t length)
{
    static const wchar_t HEX[] = L"0123456789ABCDEF";
    std::wstring result;
    result.reserve(length * 2);
    for (size_t i = 0; i < length; ++i)
    {
        result.push_back(HEX[data[i] >> 4]);
        result.push_back(HEX[data[i] & 0xF]);
    }
    return result;
}

std::wstring ConsoleUI::ToHex64(uint64_t value)
{
    uint8_t bytes[8];
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = static_cast<uint8_t>(value >> (56 - 8 * i));
    return BytesToHex(bytes, 8);
}

void ConsoleUI::WaitForKey(const std::wstring& message)
{
    wprintf(L"\n%ls", message.c_str());
    ReadKey();
    wprintf(L"\n");
}
