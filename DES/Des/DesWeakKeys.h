//  DesWeakKeys.h
//  Перевірка слабких та частково слабких ключів.
//
//  Порівняння виконується БЕЗ урахування бітів парності (8, 16, ..., 64):
//  алгоритм DES їх не використовує (функція G їх відкидає), тому, наприклад,
//  ключ 0x0000000000000000 насправді збігається зі слабким 0x0101010101010101.
//
//  Частково слабкий ключ відкидається завжди (і для DES, і для 3DES)
//  для нього існує "парний" ключ, з яким DES(DES(T, K1), K2) = T, тож
//  використовувати такий ключ ненадійно в будь-якому разі.

#pragma once

#include "../Common/Platform.h"

enum class DesKeyStrength
{
    Normal,     // ключ придатний
    Weak,       // слабкий ключ
    SemiWeak    // частково слабкий ключ
};

class DesWeakKeys
{
public:
    // pairKey - для частково слабкого ключа повертає ключ його пари
    static DesKeyStrength Check(uint64_t key, uint64_t* pairKey = nullptr);

private:
    static constexpr uint64_t PARITY_MASK = 0xFEFEFEFEFEFEFEFEull;  // без бітів парності

    static bool SameKey(uint64_t a, uint64_t b)
    {
        return (a & PARITY_MASK) == (b & PARITY_MASK);
    }

    // Слабкі ключі
    static constexpr uint64_t WEAK[4] =
    {
        0x0101010101010101ull,
        0xFEFEFEFEFEFEFEFEull,
        0x1F1F1F1F0E0E0E0Eull,
        0xE0E0E0E0F1F1F1F1ull
    };

    // Пари частково слабких ключів (K1, K2)
    static constexpr uint64_t SEMI_WEAK[6][2] =
    {
        { 0x01FE01FE01FE01FEull, 0xFE01FE01FE01FE01ull },
        { 0x1FE01FE01FE01FE0ull, 0xE0F1E0F1E0F1E0F1ull },
        { 0x01E001E001F101F1ull, 0xE001E001F101F101ull },
        { 0x1FFE1FFE0EFE0EFEull, 0xFE1FFE1FFE0EFE0Eull },
        { 0x011F011F010E010Eull, 0x1F011F010E010E01ull },
        { 0xE0FEE0FEF1FEF1FEull, 0xFEE0FEE0FEF1FEF1ull }
    };
};
