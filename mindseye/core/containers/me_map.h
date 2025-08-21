#pragma once

#include "core/me_defines.h"
#include <unordered_map>
// TODO: custom
// TODO: custom map should have stable pointers please
template <typename Key, typename Value>
struct meMap : std::unordered_map<Key, Value>
{
    bool contains(const Key& key)
    {
        return this->count(key) != 0;
    }
};

#define MEMAP_BEGIN_CUSTOM_HASHER(type, varname) \
namespace std { template <> struct hash<type> { size_t operator()(const type& varname) const

#define MEMAP_END_CUSTOM_HASHER };}