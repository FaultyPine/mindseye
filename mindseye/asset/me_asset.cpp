#include "me_asset.h"

#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"

MEEVENT_DECLARE_STATIC(registerAssetLoader);

constexpr u32 NUM_ASSET_COMPILER_THREADS = 1;

static meAssetSystem& GetAssetSystem()
{
	return *GetEngineCtx()->assetSystem;
}

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
	engine->assetSystem->assetRegistry.reserve(500);
    const CommandLineArgs& cmdline = GetCommandLineArgs();
    meAssetSetResourceDir(cmdline.hasResourceDir ? cmdline.ResourceDir : "resource/");
	engine->assetSystem->assetCompilerJobs.Initialize(&engine->engineArena, NUM_ASSET_COMPILER_THREADS);
	registerAssetLoader( meEventPayload{ &engine->engineArena });
}

void meAssetTeardown(EngineContext* engine)
{
	engine->assetSystem->assetCompilerJobs.Shutdown();
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
	meRTAsset* asset = meAssetTryGetLoaded(ident);
	constexpr u32 maxAttempts = 1000;
	u32 attempts = 0;
	while (asset->loadStage != Loaded && attempts++ < maxAttempts)
	{
		meThreadSleep(1); // TMP
		//GetAssetSystem().assetCompilerJobs.WaitOnJob(assetJobId);
		asset = meAssetTryGetLoaded(ident);
	}
	return asset ? asset->loadStage : Unloaded;
}

meAssetLoadStage* meAssetRequestLoad(
	meAllocator* allocator, 
	meAssetIdent* assetIdents, 
	u32 numAssets)
{
	meAssetSystem& assetSystem = GetAssetSystem();
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
			RWLockRead(assetSystem.assetRegistryLock);
			if (assetSystem.assetRegistry.contains(assetIdent))
			{
				const meRTAsset& loadedAsset = assetSystem.assetRegistry.at(assetIdent);
				stage = loadedAsset.loadStage;
			}
			else
			{
				struct AssetCompilerJobData
				{
					meAssetIdent ident;
					meAssetSystem* system;
				};
				// BOOKMARK: how to shape the semantics here..
				// i want to pass a pointer to some data, and the data should live as long as the job does.
				// should i pass an allocator to the job? how to do this....
				assetSystem.assetCompilerJobs.Execute( + [](void* payload) {
					meRTAsset loadedAsset = loader->meAssetLoad(assetIdent);
				}, &compilerJobData);
			}
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
	meAssetSystem& assetSystem = GetAssetSystem();
	RWLockRead(assetSystem.assetRegistryLock);
	if (!assetSystem.assetRegistry.contains(assetID))
	{
		return nullptr;
	}
	return &assetSystem.assetRegistry[assetID];
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
