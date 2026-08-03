#pragma once

#include "asset/me_asset.h"
#include "core/containers/dynarray.h"
#include "core/me_string.h"

// I don't use much AI in this project, but for writing tests I feel it's totally fine.
// I wouldn't have written tests at all anyway, so feels like a net positive.
// that is to say, expect insane weird kinda unreadable code in here  :)

struct MEREFLECT(type)
meDescriptorTestChild
{
    s32 id = 0;
    f32 weight = 0.0f;
    bool enabled = false;
};

struct MEREFLECT(type)
meDescriptorTestAsset
{
    meSerializedHeader header = {};
    String displayName = {};
    s32 health = 0;
    f32 speed = 0.0f;
    s32 samples[3] = {};
    DynArray<meDescriptorTestChild> children = {};
};

struct MEREFLECT(type)
meDescriptorLifecycleContainer
{
    String name = {};
    DynArray<String> aliases = {};
};

struct MEREFLECT(type)
meDescriptorLifecycleWithDestroy
{
    String name = {};
    DynArray<String> destroyAliases = {};

    void Destroy();
};

void meDescriptorTests();
