// ============================================================================
//  Des.cpp
// ============================================================================
#include "Des.h"
#include <cstring>

TripleDes::TripleDes(TripleDesType type, uint64_t key1, uint64_t key2, uint64_t key3)
{
    if (type == TripleDesType::EEE2 || type == TripleDesType::EDE2)
        key3 = key1;                                   // K3 = K1

    const bool middleIsDecrypt = (type == TripleDesType::EDE3 || type == TripleDesType::EDE2);

    // Ключі раундів кожного з трьох ключів: для шифрування (E) і дешифрування (D)
    DesRoundKey e1[16], d1[16], e2[16], d2[16], e3[16], d3[16];
    DesKeySchedule::Generate(key1, e1, d1);
    DesKeySchedule::Generate(key2, e2, d2);
    DesKeySchedule::Generate(key3, e3, d3);

    const size_t stageBytes = sizeof(DesRoundKey) * 16;

    // Шифрування:  E_K1 -> (E або D)_K2 -> E_K3
    std::memcpy(m_encryptKeys,      e1,                         stageBytes);
    std::memcpy(m_encryptKeys + 16, middleIsDecrypt ? d2 : e2,  stageBytes);
    std::memcpy(m_encryptKeys + 32, e3,                         stageBytes);

    // Дешифрування — обернені перетворення у зворотному порядку:
    //              D_K3 -> (D або E)_K2 -> D_K1
    std::memcpy(m_decryptKeys,      d3,                         stageBytes);
    std::memcpy(m_decryptKeys + 16, middleIsDecrypt ? e2 : d2,  stageBytes);
    std::memcpy(m_decryptKeys + 32, d1,                         stageBytes);
}
