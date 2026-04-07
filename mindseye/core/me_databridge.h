#pragma once

#include "asset/me_asset.h"
#include "scene/me_entity.h"

/*
The purpose of this system is to create an opaque way to "get some data in the game"
Being able to serialize/talk about data in this way is helpful for multiple editor systems
*/


enum RuntimeDataProvider
{
    ME_PROVIDER_ASSET,
};

struct RuntimeDataBinding
{
    RuntimeDataProvider providerType;
    union
    {
        MAID assetId;
    };
    
};
