// ============================================================================
//  BlockModes.h
//  Режими роботи блочного шифру (ECB, CBC) — НЕЗАЛЕЖНО від алгоритму.
//
//  Ідея архітектури:
//    * клас шифру (Des, TripleDes, а в ЛР4 — Aes) уміє лише одне:
//      зашифрувати / розшифрувати ОДИН блок;
//    * шаблонний клас ModeProcessor<Cipher> бере будь-який такий шифр і
//      застосовує його до цілого буфера в режимі ECB або CBC;
//    * FileProcessor (див. FileProcessor.h) працює з файлом через
//      інтерфейс IBufferProcessor і взагалі нічого не знає про DES чи AES.
//
//  Вимоги до класу Cipher (те, що використовує шаблон):
//    typename Cipher::Block                — тип одного блоку (для DES: uint64_t);
//    static constexpr size_t BLOCK_SIZE    — розмір блоку в байтах (для DES: 8);
//    Block EncryptBlock(Block) const       — шифрування одного блоку;
//    Block DecryptBlock(Block) const       — дешифрування одного блоку;
//    для типу Block має бути визначена операція ^ (XOR) — потрібна для CBC.
//  Необов'язково (для прискорення):
//    void EncryptBlocks4(Block*) const     — шифрування 4 незалежних блоків одночасно;
//    void DecryptBlocks4(Block*) const     — дешифрування 4 незалежних блоків одночасно.
//    Якщо шифр їх має, ECB і дешифрування CBC обробляють блоки по 4
//    (перевірка робиться під час компіляції — концепт SupportsFourBlocks).
//
//  Шаблон (а не віртуальні функції) обрано заради швидкодії: компілятор
//  вбудовує EncryptBlock прямо в цикл обробки буфера. Віртуальний виклик
//  відбувається лише ОДИН раз на буфер (256 КБ), а не на кожен блок.
// ============================================================================
#pragma once

#include "Platform.h"
#include <cstring>

// Чи вміє шифр обробляти 4 незалежні блоки одночасно (C++20 concept)
template <class Cipher>
concept SupportsFourBlocks = requires(const Cipher& cipher, typename Cipher::Block* blocks)
{
    cipher.EncryptBlocks4(blocks);
    cipher.DecryptBlocks4(blocks);
};

// Режим роботи блочного шифру
enum class CipherMode
{
    ECB,    // Electronic Codebook — кожен блок шифрується незалежно
    CBC     // Cipher Block Chaining — блок перед шифруванням XOR-иться з попереднім шифроблоком
};

// Що робимо з даними
enum class Direction
{
    Encrypt,
    Decrypt
};

// ----------------------------------------------------------------------------
//  IBufferProcessor — інтерфейс "обробника буфера", через який FileProcessor
//  шифрує/дешифрує дані, не знаючи, який алгоритм і режим використовуються.
// ----------------------------------------------------------------------------
class IBufferProcessor
{
public:
    virtual ~IBufferProcessor() = default;

    // Розмір блоку алгоритму в байтах (DES — 8, AES — 16)
    virtual size_t BlockSize() const = 0;

    // Обробити буфер на місці. length ОБОВ'ЯЗКОВО кратна BlockSize().
    // Буфери передаються послідовно, у порядку їх розташування у файлі,
    // тому стан ланцюжка CBC між викликами зберігається.
    virtual void Process(uint8_t* data, size_t length) = 0;
};

// ----------------------------------------------------------------------------
//  ModeProcessor<Cipher> — реалізація режимів ECB і CBC для будь-якого шифру.
// ----------------------------------------------------------------------------
template <class Cipher>
class ModeProcessor final : public IBufferProcessor
{
public:
    using Block = typename Cipher::Block;
    static constexpr size_t BLOCK_SIZE = Cipher::BLOCK_SIZE;

    // iv — початковий вектор (BLOCK_SIZE байт); для ECB не використовується
    // і може бути nullptr.
    ModeProcessor(const Cipher& cipher, CipherMode mode, Direction direction, const uint8_t* iv)
        : m_cipher(cipher), m_mode(mode), m_direction(direction)
    {
        if (iv != nullptr)
            std::memcpy(&m_chain, iv, BLOCK_SIZE);   // C(0) = IV
        else
            std::memset(&m_chain, 0, BLOCK_SIZE);
    }

    size_t BlockSize() const override { return BLOCK_SIZE; }

    void Process(uint8_t* data, size_t length) override
    {
        const size_t blockCount = length / BLOCK_SIZE;

        // Вибір гілки робиться ОДИН раз на буфер, а не на кожен блок
        if (m_mode == CipherMode::ECB)
        {
            if (m_direction == Direction::Encrypt)
                EcbEncrypt(data, blockCount);
            else
                EcbDecrypt(data, blockCount);
        }
        else
        {
            if (m_direction == Direction::Encrypt)
                CbcEncrypt(data, blockCount);
            else
                CbcDecrypt(data, blockCount);
        }
    }

private:
    // Блок читається з буфера як єдине ціле (memcpy компілятор перетворює
    // на одну інструкцію читання 64-бітного значення).
    static FORCE_INLINE Block Load(const uint8_t* p)
    {
        Block b;
        std::memcpy(&b, p, BLOCK_SIZE);
        return b;
    }

    static FORCE_INLINE void Store(uint8_t* p, const Block& b)
    {
        std::memcpy(p, &b, BLOCK_SIZE);
    }

    // Групова обробка 4 блоків (якщо шифр її підтримує)
    template <bool ENCRYPT>
    FORCE_INLINE void CryptFour(Block* blocks) const
    {
        if constexpr (ENCRYPT)
            m_cipher.EncryptBlocks4(blocks);
        else
            m_cipher.DecryptBlocks4(blocks);
    }

    // ECB: C(i) = E(T(i)) або T(i) = D(C(i)) — блоки незалежні
    template <bool ENCRYPT>
    void EcbProcess(uint8_t* data, size_t blockCount)
    {
        size_t i = 0;
        if constexpr (SupportsFourBlocks<Cipher>)
        {
            // по 4 блоки одночасно
            for (; i + 4 <= blockCount; i += 4, data += 4 * BLOCK_SIZE)
            {
                Block blocks[4];
                std::memcpy(blocks, data, 4 * BLOCK_SIZE);
                CryptFour<ENCRYPT>(blocks);
                std::memcpy(data, blocks, 4 * BLOCK_SIZE);
            }
        }
        // решта блоків (або всі, якщо групової обробки немає) — по одному
        for (; i < blockCount; ++i, data += BLOCK_SIZE)
        {
            if constexpr (ENCRYPT)
                Store(data, m_cipher.EncryptBlock(Load(data)));
            else
                Store(data, m_cipher.DecryptBlock(Load(data)));
        }
    }

    void EcbEncrypt(uint8_t* data, size_t blockCount) { EcbProcess<true>(data, blockCount); }
    void EcbDecrypt(uint8_t* data, size_t blockCount) { EcbProcess<false>(data, blockCount); }

    // CBC: C(i) = E(T(i) XOR C(i-1)),  C(0) = IV            (формули 3.3, 3.4)
    void CbcEncrypt(uint8_t* data, size_t blockCount)
    {
        Block chain = m_chain;
        for (size_t i = 0; i < blockCount; ++i, data += BLOCK_SIZE)
        {
            chain = m_cipher.EncryptBlock(Load(data) ^ chain);
            Store(data, chain);
        }
        m_chain = chain;    // зберігаємо для наступного буфера
    }

    // CBC: T(i) = D(C(i)) XOR C(i-1),  C(0) = IV            (формули 3.5, 3.6)
    // Усі шифроблоки C(i) вже відомі, тому дешифрування блоків незалежне
    // і також може виконуватися по 4 блоки одночасно.
    void CbcDecrypt(uint8_t* data, size_t blockCount)
    {
        Block chain = m_chain;
        size_t i = 0;
        if constexpr (SupportsFourBlocks<Cipher>)
        {
            for (; i + 4 <= blockCount; i += 4, data += 4 * BLOCK_SIZE)
            {
                Block cipherBlocks[4], blocks[4];
                std::memcpy(cipherBlocks, data, 4 * BLOCK_SIZE);
                std::memcpy(blocks, cipherBlocks, 4 * BLOCK_SIZE);
                m_cipher.DecryptBlocks4(blocks);

                Store(data,                  blocks[0] ^ chain);
                Store(data + BLOCK_SIZE,     blocks[1] ^ cipherBlocks[0]);
                Store(data + 2 * BLOCK_SIZE, blocks[2] ^ cipherBlocks[1]);
                Store(data + 3 * BLOCK_SIZE, blocks[3] ^ cipherBlocks[2]);
                chain = cipherBlocks[3];
            }
        }
        for (; i < blockCount; ++i, data += BLOCK_SIZE)
        {
            const Block cipherBlock = Load(data);
            Store(data, m_cipher.DecryptBlock(cipherBlock) ^ chain);
            chain = cipherBlock;
        }
        m_chain = chain;
    }

    Cipher     m_cipher;      // копія шифру з уже згенерованими ключами раундів
    CipherMode m_mode;
    Direction  m_direction;
    Block      m_chain;       // попередній шифроблок C(i-1) для режиму CBC
};
