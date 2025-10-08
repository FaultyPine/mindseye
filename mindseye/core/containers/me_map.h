#pragma once

#include "core/me_defines.h"
#include <unordered_map>
#include "core/me_string.h"
// TODO: custom map impl
// TODO: custom map should have stable pointers please
// NOTE: std::unordered_map comes with pointer stability

#define meMap std::unordered_map


#define MEMAP_BEGIN_CUSTOM_HASHER(type, varname) \
namespace std { template <> struct hash<type> { size_t operator()(const type& varname) const

#define MEMAP_END_CUSTOM_HASHER };}



MEMAP_BEGIN_CUSTOM_HASHER(StringView, obj) 
{
    size_t h1 = HashBytesL((u8*)obj.data, obj.len);
    size_t h2 = HashBytes((u8*)&obj.len, sizeof(obj.len));
    return h1 ^ (h2 << 1);
}
MEMAP_END_CUSTOM_HASHER