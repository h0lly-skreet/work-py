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
