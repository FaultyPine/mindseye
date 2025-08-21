#include "me_asset.h"

#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"

MEEVENT_DECLARE_STATIC(registerAssetLoader);

static meAssetSystem& GetAssetSystem()
{
	return *GetEngineCtx()->assetSystem;
}

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
    const CommandLineArgs& cmdline = GetCommandLineArgs();
    meAssetSetResourceDir(cmdline.hasResourceDir ? cmdline.ResourceDir : "resource/");
	registerAssetLoader( meEventPayload{ &engine->engineArena });
}

void meAssetTeardown(EngineContext* engine)
{
    MEDELETE(&engine->engineArena, meAssetSystem, engine->assetSystem);
}

void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type)
{
    meAssetSystem& assetSystem = GetAssetSystem();
	if (assetSystem.assetLoaders[type])
	{
		ME_ASSERT(false && "Not allowed to overwrite existing asset loader type");
	}
	assetSystem.assetLoaders[type] = loader;
}

meAssetLoadStage meAssetLoader::meAssetWaitForLoad(meAssetIdent ident)
{
	meAssetSystem& assetSystem = GetAssetSystem();
	meRTAsset* asset = meAssetTryGetLoaded(ident);
	constexpr u32 timeout = 99999;
	while (asset->loadStage != Loaded)
	{
		
	}
}

meAssetLoadStage* meAssetRequestLoad(
	meAllocator* allocator, 
	meAssetIdent* assetIdents, 
	u32 numAssets)
{
	meAssetLoadStage* results = MEALLOC(allocator, sizeof(meAssetLoadStage) * numAssets);
	for (u32 i = 0; i < numAssets; i++)
	{
		// dispatch to the loader for this asset type
		const meAssetIdent& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdent.id);
		const meAssetSystem& assetSystem = GetAssetSystem();
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (loader)
		{
			meAssetLoadStage loadedStage = loader->meAssetLoadDispatch(assetIdent);
			// if it's already loaded, this will indicate that
			stage = loadedStage;
		}
		results[i] = stage;
	}
    return results;
}

meAssetLoadStage* meAssetWaitForLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents,
	u32 numAssets)
{
	meAssetLoadStage* results = MEALLOC(allocator, sizeof(meAssetLoadStage) * numAssets);
    for (u32 i = 0; i < numAssets; i++)
	{
		// dispatch to the loader for this asset type
		const meAssetIdent& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdents[i].id);
		const meAssetSystem& assetSystem = GetAssetSystem();
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (loader)
		{
			meAssetLoadStage loadedStage = loader->meAssetWaitForLoad(assetIdent);
			// if it's already loaded, this will indicate that
			stage = loadedStage;
		}
		results[i] = stage;
	}
	return results;
}

meAssetLoadStage* meAssetLoadSync(
	meAllocator* allocator,
	meAssetIdent* idents,
	u32 numAssets)
{
	UNUSED_DECL meAssetLoadStage* unusedResults = meAssetRequestLoad(allocator, idents, numAssets);
	meAssetLoadStage* results = meAssetWaitForLoad(allocator, idents, numAssets);
	return results;
}

meRTAsset* meAssetTryGetLoaded(meAssetIdent assetID)
{
	if (!GetAssetSystem().assetRegistry.count(assetID))
	{
		return nullptr;
	}
	return &GetAssetSystem().assetRegistry[assetID];
}

void meAssetSetResourceDir(const char* dir)
{
    GetEngineCtx()->assetSystem->resourceDir = dir;
}

const char* meAssetGetResourceDir()
{
    return GetEngineCtx()->assetSystem->resourceDir;
}

const char* meAssetResource(const char* resourcePath)
{
    return TextFormat("%s%s%s", meAssetGetResourceDir(), meFsGetDirectorySeperator(), resourcePath);
}
