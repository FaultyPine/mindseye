#include "me_asset.h"

#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"

#include "generatedtypes/me_asset.generated.cpp"

MEEVENT_DECLARE_STATIC(registerAssetLoader);

static bool MEASSET_DEBUG_SINGLETHREADED_LOAD = 1;
constexpr u32 NUM_ASSET_COMPILER_THREADS = 1;
static StringView DEFAULT_RESOURCE_DIRECTORY_NAME = STRING_LIT("."); // cwd

static meAssetSystem& GetAssetSystem()
{
	return *GetEngineCtx()->assetSystem;
}

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
	engine->assetSystem->assetRegistry.reserve(500);
    const CommandLineArgs& cmdline = GetCommandLineArgs();
	StringView cmdlineResDir = StringView(cmdline.ResourceDir, CStringLength(cmdline.ResourceDir));
    meAssetSetResourceDir(cmdline.hasResourceDir ? cmdlineResDir : DEFAULT_RESOURCE_DIRECTORY_NAME);
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
	ME_ASSERT(assetSystem.assetLoaders[type] == nullptr && "Not allowed to overwrite existing asset loader type");
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
		const meAssetIdent& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdent.id);
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (loader)
		{
			{ // if it's already loaded, noop
				RWLockRead(assetSystem.assetRegistryLock);
				if (assetSystem.assetRegistry.find(assetIdent) != assetSystem.assetRegistry.end())
				{
					const meRTAsset& loadedAsset = assetSystem.assetRegistry.at(assetIdent);
					stage = loadedAsset.loadStage;
				}
			}
			// dispatch a request to load this asset!
			if (stage == Unloaded)
			{
				{ // add the slot in immediately, and mark it as "loading"
					meRTAsset notYetLoadedData = { .id = assetIdent.id, .type = assetType, .loadStage = Loading };
					RWLockWrite(assetSystem.assetRegistryLock);
					assetSystem.assetRegistry[assetIdent] = notYetLoadedData;
				}
				struct AssetCompilerJobData
				{
					meAssetIdent ident;
					meAssetSystem* system;
					meAssetLoader* loader;
				};
				AssetCompilerJobData jobData = {};
				jobData.ident = assetIdent;
				jobData.system = &assetSystem;
				jobData.loader = loader;
				auto loadFunc = [jobData]() 
				{
					meRTAsset loadedAsset = jobData.loader->meAssetLoad(jobData.ident);
					ME_ASSERT(loadedAsset.loadStage == Loaded && loadedAsset.loadedData.isValid());
					RWLockWrite(jobData.system->assetRegistryLock);
					jobData.system->assetRegistry[jobData.ident] = loadedAsset;
				};
				if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
				{
					loadFunc();
				}
				else
				{
					meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(loadFunc);
					UNUSED(compilerJobId);
				}
				
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
	if (assetSystem.assetRegistry.find(assetID) == assetSystem.assetRegistry.end())
	{
		return nullptr;
	}
	return &assetSystem.assetRegistry[assetID];
}

void meAssetSetResourceDir(StringView dir)
{
    GetEngineCtx()->assetSystem->resourceDir = dir;
}

StringView meAssetGetResourceDir()
{
    return GetEngineCtx()->assetSystem->resourceDir;
}

StringView meAssetResource(StringView resourcePath)
{
	meFsNormalizePathSeperators(resourcePath);
	StringView result = resourcePath;
	if (FindInString(resourcePath, meAssetGetResourceDir()) == -1)
	{
		StringView resDir = meAssetGetResourceDir();
		result = StringFormat("%.*s%.*s%.*s", STRING_VAARGS(resDir), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(resourcePath));
	}
	return result;
}
