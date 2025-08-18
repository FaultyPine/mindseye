#pragma once

#include "core/me_core.h"

struct meAssetSystem
{
    const char* resourceDir;
};

void meAssetInitialize(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);

void meAssetSetResourceDir(const char* dir);
const char* meAssetGetResourceDir();


