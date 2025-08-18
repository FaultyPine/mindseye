#pragma once

#include "core/me_defines.h"
#include <unordered_map>
// TODO: custom
template <typename Key, typename Value>
struct meMap : std::unordered_map<Key, Value>
{
    bool contains(const Key& key)
    {
        return count(key) != 0;
    }
};
