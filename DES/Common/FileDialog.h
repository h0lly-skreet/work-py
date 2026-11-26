// ============================================================================
//  FileDialog.h
//  Вибір файлів стандартними діалогами Windows
//  (функції GetOpenFileName та GetSaveFileName, як вимагає методичка).
// ============================================================================
#pragma once

#include <string>

class FileDialog
{
public:
    // Діалог відкриття існуючого файлу. false — користувач натиснув "Скасувати".
    static bool AskOpenFile(const std::wstring& title, std::wstring& path);

    // Діалог збереження. Якщо файл існує — Windows сама попросить
    // підтвердити перезапис. false — користувач натиснув "Скасувати".
    static bool AskSaveFile(const std::wstring& title, std::wstring& path);
};
