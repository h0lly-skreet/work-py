// ============================================================================
//  DesSelfTest.cpp
// ============================================================================
#include "DesSelfTest.h"
#include "Des.h"
#include "DesReference.h"
#include <cstring>

uint64_t DesSelfTest::StandardToMemory(uint64_t standardValue)
{
    uint8_t bytes[8];
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = static_cast<uint8_t>(standardValue >> (56 - 8 * i));

    uint64_t memoryValue;
    std::memcpy(&memoryValue, bytes, 8);
    return memoryValue;
}

uint64_t DesSelfTest::NextRandom()
{
    m_randomState ^= m_randomState << 13;
    m_randomState ^= m_randomState >> 7;
    m_randomState ^= m_randomState << 17;
    return m_randomState;
}

bool DesSelfTest::CheckVector(uint64_t key, uint64_t plain, uint64_t cipher,
                              const wchar_t* name, std::wstring& error)
{
    const Des des(key);
    const bool referenceOk = DesReference::Encrypt(plain, key) == cipher &&
                             DesReference::Decrypt(cipher, key) == plain;
    const bool optimizedOk = des.EncryptBlock(StandardToMemory(plain)) == StandardToMemory(cipher) &&
                             des.DecryptBlock(StandardToMemory(cipher)) == StandardToMemory(plain);
    if (!referenceOk || !optimizedOk)
    {
        error = std::wstring(L"не пройдено тест-вектор: ") + name;
        return false;
    }
    return true;
}

bool DesSelfTest::Run(std::wstring& error)
{
    // 1-2. Відомі тест-вектори
    if (!CheckVector(0x5B5A57676A56676Eull, 0x675A69675E5A6B5Aull, 0x974AFFBF86022D1Full,
                     L"des_test.txt (викладача)", error))
        return false;
    if (!CheckVector(0x133457799BBCDFF1ull, 0x0123456789ABCDEFull, 0x85E813540F0AB405ull,
                     L"FIPS (0123456789ABCDEF)", error))
        return false;

    // 3-5. Випадкові ключі та блоки
    for (int i = 0; i < 10000; ++i)
    {
        const uint64_t k1 = NextRandom(), k2 = NextRandom(), k3 = NextRandom();
        const uint64_t plain = NextRandom();
        const uint64_t plainMem = StandardToMemory(plain);

        const Des des(k1);
        const uint64_t cipherMem = des.EncryptBlock(plainMem);
        if (cipherMem != StandardToMemory(DesReference::Encrypt(plain, k1)))
        {
            error = L"оптимізований DES не збігається з еталонним";
            return false;
        }
        if (des.DecryptBlock(cipherMem) != plainMem)
        {
            error = L"DES: дешифрування не відновлює вхідний блок";
            return false;
        }

        const TripleDesType types[] = { TripleDesType::EEE3, TripleDesType::EDE3,
                                        TripleDesType::EEE2, TripleDesType::EDE2 };
        for (TripleDesType type : types)
        {
            const TripleDes tdes(type, k1, k2, k3);
            if (tdes.DecryptBlock(tdes.EncryptBlock(plainMem)) != plainMem)
            {
                error = L"3DES: дешифрування не відновлює вхідний блок";
                return false;
            }
        }

        // Обробка 4 блоків одночасно (EncryptBlocks4 / DecryptBlocks4)
        // дає той самий результат, що й обробка по одному блоку
        {
            const uint64_t source[4] = { plainMem, NextRandom(), NextRandom(), NextRandom() };
            const TripleDes tdes(TripleDesType::EDE3, k1, k2, k3);

            uint64_t desFour[4], tdesFour[4];
            for (int j = 0; j < 4; ++j)
                desFour[j] = tdesFour[j] = source[j];

            des.EncryptBlocks4(desFour);
            tdes.EncryptBlocks4(tdesFour);
            for (int j = 0; j < 4; ++j)
            {
                if (desFour[j] != des.EncryptBlock(source[j]) || tdesFour[j] != tdes.EncryptBlock(source[j]))
                {
                    error = L"шифрування 4 блоків одночасно не збігається з поблочним";
                    return false;
                }
            }

            des.DecryptBlocks4(desFour);
            tdes.DecryptBlocks4(tdesFour);
            for (int j = 0; j < 4; ++j)
            {
                if (desFour[j] != source[j] || tdesFour[j] != source[j])
                {
                    error = L"дешифрування 4 блоків одночасно не відновлює вхідні блоки";
                    return false;
                }
            }
        }

        // EDE з однаковими ключами: E_K(D_K(E_K(T))) = E_K(T)
        const TripleDes sameKeys(TripleDesType::EDE3, k1, k1, k1);
        if (sameKeys.EncryptBlock(plainMem) != cipherMem)
        {
            error = L"3DES-EDE з однаковими ключами не дорівнює DES";
            return false;
        }

        // EEE3 = три послідовні шифрування DES
        const TripleDes eee(TripleDesType::EEE3, k1, k2, k3);
        const Des des2(k2), des3(k3);
        if (eee.EncryptBlock(plainMem) != des3.EncryptBlock(des2.EncryptBlock(cipherMem)))
        {
            error = L"3DES-EEE3 не дорівнює трьом послідовним DES";
            return false;
        }
    }

    return true;
}
