// ============================================================================
//  DesReference.h
//  ЕТАЛОННА (неоптимізована) реалізація DES — пряма реалізація схем
//  рис. 3.3 і 3.4 методички зі стандартними таблицями, біт за бітом.
//
//  Вона повільна і НЕ використовується для шифрування файлів. Її єдине
//  призначення — самоперевірка при запуску програми: результати
//  оптимізованої реалізації (DesCore) порівнюються з еталонною. Це доводить,
//  що всі оптимізовані таблиці розраховані правильно.
//
//  Блок і ключ — 64-бітні числа у стандартному вигляді (біт №1 — старший).
// ============================================================================
#pragma once

#include "../Common/Platform.h"

class DesReference
{
public:
    static uint64_t Encrypt(uint64_t block, uint64_t key) { return Crypt(block, key, false); }
    static uint64_t Decrypt(uint64_t block, uint64_t key) { return Crypt(block, key, true); }

private:
    static uint64_t Crypt(uint64_t block, uint64_t key, bool decrypt);
    static uint32_t F(uint32_t r, uint64_t roundKey48);
};
