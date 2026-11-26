// ============================================================================
//  Des.h
//  Класи шифрів DES та 3DES. Обидва задовольняють вимоги шаблону
//  ModeProcessor<Cipher> (див. Common/BlockModes.h): мають тип Block,
//  константу BLOCK_SIZE і методи EncryptBlock / DecryptBlock, а також
//  необов'язкові методи EncryptBlocks4 / DecryptBlocks4 (4 блоки одночасно)
//  і CbcEncrypt (прискорене шифрування в режимі CBC).
//
//  Ключ задається 64-бітним числом у стандартному вигляді: 1-й введений
//  байт ключа — старший байт числа (так його й перетворює функція
//  введення: кожна пара hex-цифр — черговий байт).
// ============================================================================
#pragma once

#include "DesCore.h"

// ----------------------------------------------------------------------------
//  DES
// ----------------------------------------------------------------------------
class Des
{
public:
    using Block = uint64_t;
    static constexpr size_t BLOCK_SIZE = 8;

    explicit Des(uint64_t key)
    {
        DesKeySchedule::Generate(key, m_encryptKeys, m_decryptKeys);
    }

    FORCE_INLINE Block EncryptBlock(Block block) const
    {
        return DesCore::Crypt<1>(block, m_encryptKeys);
    }

    FORCE_INLINE Block DecryptBlock(Block block) const
    {
        return DesCore::Crypt<1>(block, m_decryptKeys);
    }

    // Обробка 4 незалежних блоків одночасно (див. DesCore::Crypt4)
    FORCE_INLINE void EncryptBlocks4(Block* blocks) const { DesCore::Crypt4<1>(blocks, m_encryptKeys); }
    FORCE_INLINE void DecryptBlocks4(Block* blocks) const { DesCore::Crypt4<1>(blocks, m_decryptKeys); }

    // Шифрування в режимі CBC зі скороченим ланцюжком (див. DesCore::CbcEncrypt)
    void CbcEncrypt(uint8_t* data, size_t blockCount, Block& chain) const
    {
        DesCore::CbcEncrypt<1>(data, blockCount, chain, m_encryptKeys);
    }

private:
    DesRoundKey m_encryptKeys[16];   // K(1)..K(16)
    DesRoundKey m_decryptKeys[16];   // K(16)..K(1)
};

// ----------------------------------------------------------------------------
//  3DES
// ----------------------------------------------------------------------------
enum class TripleDesType
{
    EEE3,   // C = E_K3( E_K2( E_K1(T) ) )              — 3 різні ключі   (3.1)
    EDE3,   // C = E_K3( D_K2( E_K1(T) ) )              — 3 різні ключі   (3.2)
    EEE2,   // як EEE3, але K3 = K1                     — 2 ключі
    EDE2    // як EDE3, але K3 = K1                     — 2 ключі
};

class TripleDes
{
public:
    using Block = uint64_t;
    static constexpr size_t BLOCK_SIZE = 8;

    // Кількість ключів, які треба ввести для заданого типу (2 або 3)
    static unsigned KeyCount(TripleDesType type)
    {
        return (type == TripleDesType::EEE3 || type == TripleDesType::EDE3) ? 3 : 2;
    }

    // Для типів EEE2 / EDE2 параметр key3 ігнорується (використовується K1)
    TripleDes(TripleDesType type, uint64_t key1, uint64_t key2, uint64_t key3);

    FORCE_INLINE Block EncryptBlock(Block block) const
    {
        return DesCore::Crypt<3>(block, m_encryptKeys);
    }

    FORCE_INLINE Block DecryptBlock(Block block) const
    {
        return DesCore::Crypt<3>(block, m_decryptKeys);
    }

    // Обробка 4 незалежних блоків одночасно (див. DesCore::Crypt4)
    FORCE_INLINE void EncryptBlocks4(Block* blocks) const { DesCore::Crypt4<3>(blocks, m_encryptKeys); }
    FORCE_INLINE void DecryptBlocks4(Block* blocks) const { DesCore::Crypt4<3>(blocks, m_decryptKeys); }

    // Шифрування в режимі CBC зі скороченим ланцюжком (див. DesCore::CbcEncrypt)
    void CbcEncrypt(uint8_t* data, size_t blockCount, Block& chain) const
    {
        DesCore::CbcEncrypt<3>(data, blockCount, chain, m_encryptKeys);
    }

private:
    // 48 ключів раундів: 3 етапи по 16.
    // Етап "E з ключем K" = ключі K(1)..K(16), етап "D з ключем K" = K(16)..K(1).
    DesRoundKey m_encryptKeys[48];
    DesRoundKey m_decryptKeys[48];
};
