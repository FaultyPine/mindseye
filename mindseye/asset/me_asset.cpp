#include "me_asset.h"

#include "core/me_cmdline.h"

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
    const CommandLineArgs& cmdline = GetCommandLineArgs();
    meAssetSetResourceDir(cmdline.hasResourceDir ? cmdline.ResourceDir : "resource/");
}

void meAssetTeardown(EngineContext* engine)
{
    MEDELETE(&engine->engineArena, meAssetSystem, engine->assetSystem);
}

void meAssetSetResourceDir(const char* dir)
{
    GetEngineCtx()->assetSystem->resourceDir = dir;
}

const char* meAssetGetResourceDir()
{
    return GetEngineCtx()->assetSystem->resourceDir;
}