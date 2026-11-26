//  DesOptimizedTables.cpp
//  РОЗРАХУНОК оптимізованих таблиць зі стандартних.

#include "DesOptimizedTables.h"
#include "DesStandardTables.h"

// Циклічні зсуви кожної 32-бітної половини 64-бітного числа на 1 біт
static uint64_t RotateHalvesRight1(uint64_t value)
{
    const uint32_t high = static_cast<uint32_t>(value >> 32);
    const uint32_t low  = static_cast<uint32_t>(value);
    return (static_cast<uint64_t>(RotateRight32(high, 1)) << 32) | RotateRight32(low, 1);
}

static uint64_t RotateHalvesLeft1(uint64_t value)
{
    const uint32_t high = static_cast<uint32_t>(value >> 32);
    const uint32_t low  = static_cast<uint32_t>(value);
    return (static_cast<uint64_t>(RotateRight32(high, 31)) << 32) | RotateRight32(low, 31);
}

uint32_t DesOptimizedTables::SP[8][64];
uint64_t DesOptimizedTables::IP[8][256];
uint64_t DesOptimizedTables::FP[8][256];

// Статична ініціалізація, Build() виконується до початку main()
const bool DesOptimizedTables::s_isBuilt = DesOptimizedTables::Build();

bool DesOptimizedTables::Build()
{
    BuildSP();
    BuildIP();
    BuildFP();
    return true;
}

//  SP[i][x]: x — 6-бітне значення, що надходить на вхід S-box №(i+1).
//
//  1. y = S_{i+1}(x) — 4-бітний результат (за правилом рядок/стовпець).
//  2. До перестановки P 32-бітне значення складається з 8 тетрад:
//     S1 дає біти 1..4, S2 — біти 5..8, ..., S8 — біти 29..32.
//     Тобто результат S-box (i+1) займає біти 4i+1..4i+4, а в числі
//     (нумерація з 0 справа) це зсув на 28 - 4i.
//  3. До цього 32-бітного значення, де заповнена лише одна тетрада,
//     застосовуємо стандартну перестановку P.
//  4. Результат зсуваємо ROTR 1 — у такому вигляді зберігаються L і R.
//  Оскільки P лише переставляє біти, а тетради різних S-box не
//  перетинаються, повний результат F = OR (або XOR) восьми таких значень.

void DesOptimizedTables::BuildSP()
{
    for (unsigned box = 0; box < 8; ++box)
    {
        for (unsigned x = 0; x < 64; ++x)
        {
            const uint64_t sOut      = DesStandardTables::SBox(box, x);
            const uint64_t beforeP   = sOut << (28 - 4 * box);
            const uint32_t afterP    = static_cast<uint32_t>(
                DesStandardTables::Permute(beforeP, 32, DesStandardTables::P, 32));
            SP[box][x] = RotateRight32(afterP, 1);
        }
    }
}

//  IP[j][v]: внесок j-го байта ФАЙЛУ (значення v) у результат IP.
//
//  j-й байт файлу — це біти 8j+1..8j+8 блоку за стандартом, тобто в
//  "стандартному" 64-бітному числі він стоїть на зсуві 56 - 8j.
//  Застосовуємо до такого числа (де заповнений лише один байт) стандартну
//  перестановку IP. Результат — 64-бітне число: старші 32 біти = L(0),
//  молодші 32 біти = R(0); кожна половина одразу зсувається ROTR 1.
//
//  При шифруванні: x — блок, прочитаний з файлу як uint64 (байт j файлу =
//  біти 8j..8j+7 числа на x64), тоді  IP(x) = OR_j IP[j][(x >> 8j) & 0xFF].

void DesOptimizedTables::BuildIP()
{
    for (unsigned j = 0; j < 8; ++j)
    {
        for (unsigned v = 0; v < 256; ++v)
        {
            const uint64_t standardValue = static_cast<uint64_t>(v) << (56 - 8 * j);
            IP[j][v] = RotateHalvesRight1(
                DesStandardTables::Permute(standardValue, 64, DesStandardTables::IP, 64));
        }
    }
}

//  FP[j][v]: внесок j-го байта (значення v) вхідного числа R(16)L(16) у
//  результат кінцевої перестановки IP^-1.
//
//  Вхід тут — внутрішнє 64-бітне число (не з файлу), тому j-й байт числа
//  стоїть на зсуві 8j. Половини цього числа зсунуті ROTR 1, тому спочатку
//  повертаємо їх назад (ROTL 1). Після стандартної перестановки FP отримуємо
//  результат у "стандартному" вигляді (1-й байт блоку — старший), а записати
//  у файл його треба як uint64 на little-endian машині, тобто з
//  інвертованим порядком байт. Тому інвертування порядку байт (ByteSwap)
//  виконується ТУТ, при розрахунку таблиці, а не для кожного блоку.

static uint64_t ByteSwap64(uint64_t value)
{
    uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        result = (result << 8) | (value & 0xFF);
        value >>= 8;
    }
    return result;
}

void DesOptimizedTables::BuildFP()
{
    for (unsigned j = 0; j < 8; ++j)
    {
        for (unsigned v = 0; v < 256; ++v)
        {
            const uint64_t internalValue = static_cast<uint64_t>(v) << (8 * j);
            const uint64_t standardResult =
                DesStandardTables::Permute(RotateHalvesLeft1(internalValue), 64, DesStandardTables::FP, 64);
            FP[j][v] = ByteSwap64(standardResult);
        }
    }
}
