// ============================================================================
//  DesKeySchedule.cpp
// ============================================================================
#include "DesKeySchedule.h"
#include "DesStandardTables.h"

// Циклічний зсув 28-бітного значення ліворуч на shift бітів
static inline uint32_t RotateLeft28(uint32_t value, unsigned shift)
{
    return ((value << shift) | (value >> (28 - shift))) & 0x0FFFFFFFu;
}

void DesKeySchedule::GenerateRaw(uint64_t key, uint64_t rawKeys[ROUNDS])
{
    // а) функція G: 64 біти -> 56 біт (біти парності 8,16,...,64 відкидаються)
    const uint64_t permuted = DesStandardTables::Permute(key, 64, DesStandardTables::G, 56);

    // б) C(0) — старші 28 біт, D(0) — молодші 28 біт
    uint32_t c = static_cast<uint32_t>(permuted >> 28) & 0x0FFFFFFFu;
    uint32_t d = static_cast<uint32_t>(permuted)       & 0x0FFFFFFFu;

    // в) для кожного раунду: циклічний зсув на 1 або 2 біти (табл. 3.13),
    //    потім функція H: 56 біт C(i)D(i) -> 48-бітний ключ K(i)
    for (unsigned round = 0; round < ROUNDS; ++round)
    {
        c = RotateLeft28(c, DesStandardTables::SHIFTS[round]);
        d = RotateLeft28(d, DesStandardTables::SHIFTS[round]);

        const uint64_t cd = (static_cast<uint64_t>(c) << 28) | d;
        rawKeys[round] = DesStandardTables::Permute(cd, 56, DesStandardTables::H, 48);
    }
}

DesRoundKey DesKeySchedule::ToOptimized(uint64_t rawKey48)
{
    DesRoundKey result{ 0, 0 };
    for (unsigned group = 1; group <= 8; ++group)
    {
        // 6-бітна частина ключа для S-box №group (біти 6*group-5 .. 6*group)
        const uint32_t part = static_cast<uint32_t>(rawKey48 >> (48 - 6 * group)) & 0x3F;

        if (group % 2 == 1)
        {
            // S1, S3, S5, S7: група в числі U на зсуві 30 - 4*group (26, 18, 10, 2)
            result.uGroups |= part << (30 - 4 * group);
        }
        else
        {
            // S2, S4, S6, S8: група в числі V на зсуві (26 - 4*group) mod 32 (18, 10, 2, 26)
            result.vGroups |= part << ((26 - 4 * group) & 31);
        }
    }
    return result;
}

void DesKeySchedule::Generate(uint64_t key, DesRoundKey encryptKeys[ROUNDS],
                              DesRoundKey decryptKeys[ROUNDS])
{
    uint64_t raw[ROUNDS];
    GenerateRaw(key, raw);

    for (unsigned round = 0; round < ROUNDS; ++round)
    {
        encryptKeys[round] = ToOptimized(raw[round]);
        // дешифрування: ті самі ключі у зворотному порядку
        decryptKeys[ROUNDS - 1 - round] = encryptKeys[round];
    }
}
