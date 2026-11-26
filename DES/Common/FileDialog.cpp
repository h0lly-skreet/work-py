// ============================================================================
//  FileDialog.cpp
// ============================================================================
#include "FileDialog.h"

#include <windows.h>
#include <commdlg.h>
#include <iterator>

#pragma comment(lib, "comdlg32.lib")

namespace
{
    // Фільтр типів файлів: пари "опис\0шаблон\0", у кінці подвійний \0.
    // Файли не обов'язково текстові, тому за замовчуванням — усі файли.
    const wchar_t FILE_FILTER[] = L"Усі файли (*.*)\0*.*\0Текстові файли (*.txt)\0*.txt\0";

    OPENFILENAMEW MakeDialogParams(const std::wstring& title, wchar_t* buffer, DWORD bufferSize)
    {
        OPENFILENAMEW params = {};
        params.lStructSize  = sizeof(params);
        params.hwndOwner    = GetConsoleWindow();   // діалог з'являється поверх консолі
        params.lpstrFilter  = FILE_FILTER;
        params.nFilterIndex = 1;
        params.lpstrFile    = buffer;
        params.nMaxFile     = bufferSize;
        params.lpstrTitle   = title.c_str();
        return params;
    }
}

bool FileDialog::AskOpenFile(const std::wstring& title, std::wstring& path)
{
    wchar_t buffer[MAX_PATH * 4] = {};
    OPENFILENAMEW params = MakeDialogParams(title, buffer, static_cast<DWORD>(std::size(buffer)));
    params.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&params))
        return false;

    path = buffer;
    return true;
}

bool FileDialog::AskSaveFile(const std::wstring& title, std::wstring& path)
{
    wchar_t buffer[MAX_PATH * 4] = {};
    OPENFILENAMEW params = MakeDialogParams(title, buffer, static_cast<DWORD>(std::size(buffer)));
    params.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&params))
        return false;

    path = buffer;
    return true;
}
