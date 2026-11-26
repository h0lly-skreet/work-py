// ============================================================================
//  DesReference.cpp
// ============================================================================
#include "DesReference.h"
#include "DesStandardTables.h"
#include "DesKeySchedule.h"

using T = DesStandardTables;

// F(R(i-1), K(i)) строго за рис. 3.4
uint32_t DesReference::F(uint32_t r, uint64_t roundKey48)
{
    // а) розширення 32 -> 48 функцією E
    const uint64_t expanded = T::Permute(r, 32, T::E, 48);

    // б) XOR з ключем раунду
    const uint64_t mixed = expanded ^ roundKey48;

    // в) 8 S-box: кожні 6 біт -> 4 біти
    uint32_t sOut = 0;
    for (unsigned box = 0; box < 8; ++box)
    {
        const unsigned six = static_cast<unsigned>(mixed >> (42 - 6 * box)) & 0x3F;
        sOut = (sOut << 4) | T::SBox(box, six);
    }

    // г) перестановка P
    return static_cast<uint32_t>(T::Permute(sOut, 32, T::P, 32));
}

// Шифрування блоку строго за рис. 3.3
uint64_t DesReference::Crypt(uint64_t block, uint64_t key, bool decrypt)
{
    uint64_t roundKeys[16];
    DesKeySchedule::GenerateRaw(key, roundKeys);

    // 1. початкова перестановка IP
    const uint64_t ip = T::Permute(block, 64, T::IP, 64);
    uint32_t left  = static_cast<uint32_t>(ip >> 32);
    uint32_t right = static_cast<uint32_t>(ip);

    // 2. 16 раундів: L(i) = R(i-1), R(i) = L(i-1) XOR F(R(i-1), K(i))
    //    при дешифруванні ключі в зворотному порядку
    for (unsigned i = 0; i < 16; ++i)
    {
        const uint64_t k = decrypt ? roundKeys[15 - i] : roundKeys[i];
        const uint32_t newRight = left ^ F(right, k);
        left  = right;
        right = newRight;
    }

    // 3. обмін половин: R(16)L(16)
    const uint64_t preOutput = (static_cast<uint64_t>(right) << 32) | left;

    // 4. кінцева перестановка IP^-1
    return T::Permute(preOutput, 64, T::FP, 64);
}
