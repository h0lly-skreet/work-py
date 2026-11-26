//  DesWeakKeys.cpp

#include "DesWeakKeys.h"

DesKeyStrength DesWeakKeys::Check(uint64_t key, uint64_t* pairKey)
{
    for (uint64_t weak : WEAK)
    {
        if (SameKey(key, weak))
            return DesKeyStrength::Weak;
    }

    for (const auto& pair : SEMI_WEAK)
    {
        for (int i = 0; i < 2; ++i)
        {
            if (SameKey(key, pair[i]))
            {
                if (pairKey != nullptr)
                    *pairKey = pair[1 - i];
                return DesKeyStrength::SemiWeak;
            }
        }
    }

    return DesKeyStrength::Normal;
}
