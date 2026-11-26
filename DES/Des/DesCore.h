// ============================================================================
//  DesCore.h
//  Оптимізоване шифрування ОДНОГО 64-бітного блоку (мережа Фейстеля DES).
//  Спільне ядро для DES та 3DES.
//
//  Усі функції вбудовуються (FORCE_INLINE) у цикл обробки буфера.
// ============================================================================
#pragma once

#include "../Common/Platform.h"
#include "DesOptimizedTables.h"
#include "DesKeySchedule.h"

class DesCore
{
public:
    // ------------------------------------------------------------------------
    //  Шифрування/дешифрування блоку.
    //  STAGES = 1 — звичайний DES (16 раундів),
    //  STAGES = 3 — 3DES (3 x 16 = 48 раундів).
    //  keys — STAGES*16 ключів раундів у порядку застосування.
    //  Шифрування від дешифрування відрізняється ЛИШЕ порядком ключів.
    //
    //  Оптимізація для 3DES: між етапами DES кінцева перестановка IP^-1
    //  одного етапу і початкова IP наступного взаємно знищуються
    //  (IP(IP^-1(x)) = x), тому IP виконується один раз на початку,
    //  IP^-1 — один раз у кінці, а між етапами лише міняються місцями
    //  половини блоку (що для компілятора — просто перейменування змінних).
    // ------------------------------------------------------------------------
    template <unsigned STAGES>
    static FORCE_INLINE uint64_t Crypt(uint64_t block, const DesRoundKey* keys)
    {
        const uint64_t permuted = InitialPermutation(block);   // етап 1
        uint32_t left  = static_cast<uint32_t>(permuted >> 32); // L(0)
        uint32_t right = static_cast<uint32_t>(permuted);       // R(0)

        for (unsigned stage = 0; stage < STAGES; ++stage)
        {
            Rounds16(left, right, keys + 16 * stage);           // етап 2
            const uint32_t tmp = left; left = right; right = tmp; // етап 3: обмін половин
        }

        return FinalPermutation((static_cast<uint64_t>(left) << 32) | right); // етап 4
    }

    // ------------------------------------------------------------------------
    //  Шифрування/дешифрування ЧОТИРЬОХ незалежних блоків одночасно.
    //
    //  Раунди DES одного блоку виконуються строго послідовно: кожен раунд
    //  чекає результат попереднього (а кожне звертання до таблиці SP — це
    //  кілька тактів очікування пам'яті). Сучасний процесор вміє виконувати
    //  кілька незалежних команд одночасно (суперскалярність), але в одному
    //  ланцюжку раундів незалежних команд мало, і частина блоків процесора
    //  простоює. Якщо ж раунди чотирьох РІЗНИХ блоків чергуються, процесор
    //  виконує їх паралельно — і швидкість зростає майже вдвічі.
    //
    //  Застосовується там, де блоки незалежні: ECB (шифрування і
    //  дешифрування) та дешифрування CBC. Шифрування CBC так прискорити
    //  неможливо: кожен блок залежить від шифроблоку попереднього.
    // ------------------------------------------------------------------------
    template <unsigned STAGES>
    static FORCE_INLINE void Crypt4(uint64_t* blocks, const DesRoundKey* keys)
    {
        const uint64_t p0 = InitialPermutation(blocks[0]);
        const uint64_t p1 = InitialPermutation(blocks[1]);
        const uint64_t p2 = InitialPermutation(blocks[2]);
        const uint64_t p3 = InitialPermutation(blocks[3]);

        uint32_t l0 = static_cast<uint32_t>(p0 >> 32), r0 = static_cast<uint32_t>(p0);
        uint32_t l1 = static_cast<uint32_t>(p1 >> 32), r1 = static_cast<uint32_t>(p1);
        uint32_t l2 = static_cast<uint32_t>(p2 >> 32), r2 = static_cast<uint32_t>(p2);
        uint32_t l3 = static_cast<uint32_t>(p3 >> 32), r3 = static_cast<uint32_t>(p3);

        for (unsigned stage = 0; stage < STAGES; ++stage)
        {
            const DesRoundKey* k = keys + 16 * stage;
            for (unsigned i = 0; i < 16; i += 2)
            {
                l0 ^= F(r0, k[i]);      l1 ^= F(r1, k[i]);
                l2 ^= F(r2, k[i]);      l3 ^= F(r3, k[i]);
                r0 ^= F(l0, k[i + 1]);  r1 ^= F(l1, k[i + 1]);
                r2 ^= F(l2, k[i + 1]);  r3 ^= F(l3, k[i + 1]);
            }
            // обмін половин після 16 раундів
            uint32_t t;
            t = l0; l0 = r0; r0 = t;
            t = l1; l1 = r1; r1 = t;
            t = l2; l2 = r2; r2 = t;
            t = l3; l3 = r3; r3 = t;
        }

        blocks[0] = FinalPermutation((static_cast<uint64_t>(l0) << 32) | r0);
        blocks[1] = FinalPermutation((static_cast<uint64_t>(l1) << 32) | r1);
        blocks[2] = FinalPermutation((static_cast<uint64_t>(l2) << 32) | r2);
        blocks[3] = FinalPermutation((static_cast<uint64_t>(l3) << 32) | r3);
    }

private:
    // IP через 8 таблиць по 256 елементів (байт числа = індекс таблиці)
    static FORCE_INLINE uint64_t InitialPermutation(uint64_t x)
    {
        const auto& T = DesOptimizedTables::IP;
        return T[0][ x        & 0xFF] | T[1][(x >>  8) & 0xFF] |
               T[2][(x >> 16) & 0xFF] | T[3][(x >> 24) & 0xFF] |
               T[4][(x >> 32) & 0xFF] | T[5][(x >> 40) & 0xFF] |
               T[6][(x >> 48) & 0xFF] | T[7][ x >> 56        ];
    }

    // IP^-1 через 8 таблиць по 256 елементів (з урахуванням порядку байт файлу)
    static FORCE_INLINE uint64_t FinalPermutation(uint64_t x)
    {
        const auto& T = DesOptimizedTables::FP;
        return T[0][ x        & 0xFF] | T[1][(x >>  8) & 0xFF] |
               T[2][(x >> 16) & 0xFF] | T[3][(x >> 24) & 0xFF] |
               T[4][(x >> 32) & 0xFF] | T[5][(x >> 40) & 0xFF] |
               T[6][(x >> 48) & 0xFF] | T[7][ x >> 56        ];
    }

    // ------------------------------------------------------------------------
    //  Функція F(R, K) — рис. 3.4 методички, оптимізований варіант.
    //
    //  1. Розширення E без таблиці: W = [R32 | R1..R32 | R1] — 34 біти.
    //     Група для S-box №i — 6 біт W на зсуві 32 - 4i (див. табл. E:
    //     кожна група — це 4 біти R плюс по одному сусідньому біту з кожного
    //     боку). Тобто розширення 32->48 виконується груповими зсувами.
    //  2. XOR з ключем раунду — двома операціями (див. DesRoundKey).
    //  3. S-box + P — 8 звертань до об'єднаних таблиць SP.
    // ------------------------------------------------------------------------
    static FORCE_INLINE uint32_t F(uint32_t r, const DesRoundKey& key)
    {
        const uint64_t w = (static_cast<uint64_t>(r & 1u) << 33) |
                           (static_cast<uint64_t>(r) << 1)        |
                           (r >> 31);

        const uint64_t odd  = w ^ key.oddGroups;    // групи для S1, S3, S5, S7
        const uint64_t even = w ^ key.evenGroups;   // групи для S2, S4, S6, S8

        const auto& SP = DesOptimizedTables::SP;
        return SP[0][(odd  >> 28) & 0x3F] | SP[1][(even >> 24) & 0x3F] |
               SP[2][(odd  >> 20) & 0x3F] | SP[3][(even >> 16) & 0x3F] |
               SP[4][(odd  >> 12) & 0x3F] | SP[5][(even >>  8) & 0x3F] |
               SP[6][(odd  >>  4) & 0x3F] | SP[7][ even        & 0x3F];
    }

    // ------------------------------------------------------------------------
    //  16 раундів мережі Фейстеля, цикл розкрито (п. 6 базової оптимізації).
    //  Замість обміну половин після кожного раунду (L(i) = R(i-1),
    //  R(i) = L(i-1) XOR F(R(i-1), K(i))) половини просто по черзі
    //  "міняються ролями": після парної кількості раундів
    //  left = L(16), right = R(16).
    // ------------------------------------------------------------------------
    static FORCE_INLINE void Rounds16(uint32_t& left, uint32_t& right, const DesRoundKey* k)
    {
        uint32_t l = left, r = right;
        l ^= F(r, k[ 0]);  r ^= F(l, k[ 1]);
        l ^= F(r, k[ 2]);  r ^= F(l, k[ 3]);
        l ^= F(r, k[ 4]);  r ^= F(l, k[ 5]);
        l ^= F(r, k[ 6]);  r ^= F(l, k[ 7]);
        l ^= F(r, k[ 8]);  r ^= F(l, k[ 9]);
        l ^= F(r, k[10]);  r ^= F(l, k[11]);
        l ^= F(r, k[12]);  r ^= F(l, k[13]);
        l ^= F(r, k[14]);  r ^= F(l, k[15]);
        left = l; right = r;
    }
};
