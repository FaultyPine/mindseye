#include "me_map.h"

// TODO: more detailed/edge-casey ones
void meMapTests()
{
    meMap<u32, s32> map;
    map[1] = 10;
    map[2] = 20;
    ME_ASSERT(map.size == 2);
    ME_ASSERT(map[1] == 10);
    ME_ASSERT(map[2] == 20);

    map[1] = 15;
    ME_ASSERT(map.size == 2);
    ME_ASSERT(map[1] == 15);

    s32 valueSum = 0;
    for (const auto& [key, value] : map)
    {
        UNUSED(key);
        valueSum += value;
    }
    ME_ASSERT(valueSum == 35);
}