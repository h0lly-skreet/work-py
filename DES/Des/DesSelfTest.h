//  DesSelfTest.h
//  Самоперевірка реалізації при запуску програми:
//   1) тест-вектор Єгорова (des_test.txt):
//        ключ 5B5A57676A56676E, текст 675A69675E5A6B5A -> 974AFFBF86022D1F;
//   2) класичний тест-вектор:
//        ключ 133457799BBCDFF1, текст 0123456789ABCDEF -> 85E813540F0AB405;
//   3) 10000 випадкових пар (ключ, блок): оптимізований DES == еталонний;
//   4) для DES і всіх 4-х типів 3DES: D(E(T)) == T;
//   5) 3DES з K1 = K2 = K3: EDE еквівалентний одинарному DES;
//   6) обробка 4 блоків одночасно збігається з поблочною;
//   7) прискорене шифрування CBC збігається з формулою режиму CBC.

#pragma once

#include "../Common/Platform.h"
#include <string>

class DesSelfTest
{
public:
    // Повертає true, якщо всі перевірки пройдено, інакше — опис помилки в error
    bool Run(std::wstring& error);

private:
    // Перетворення блоку зі стандартного вигляду (1-й байт — старший) у
    // вигляд, у якому блок читається з файлу (байти в порядку розміщення в пам'яті)
    static uint64_t StandardToMemory(uint64_t standardValue);

    // Псевдовипадкові числа (xorshift64) — щоб тест був відтворюваним
    uint64_t NextRandom();

    bool CheckVector(uint64_t key, uint64_t plain, uint64_t cipher, const wchar_t* name, std::wstring& error);

    uint64_t m_randomState = 0x9E3779B97F4A7C15ull;
};
