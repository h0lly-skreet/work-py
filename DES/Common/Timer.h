// ============================================================================
//  Timer.h
//  Простий секундомір для вимірювання часу шифрування/дешифрування.
//  steady_clock — монотонний годинник високої точності (на Windows він
//  реалізований через QueryPerformanceCounter).
// ============================================================================
#pragma once

#include <chrono>

class Timer
{
public:
    Timer() { Start(); }

    void Start() { m_start = std::chrono::steady_clock::now(); }

    // Час від останнього Start() у секундах
    double ElapsedSeconds() const
    {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - m_start).count();
    }

private:
    std::chrono::steady_clock::time_point m_start;
};
