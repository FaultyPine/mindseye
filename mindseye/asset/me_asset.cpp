#include "core/thread/me_thread.h"
#include "core/me_serialize.h"
#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_asset.h"

#include "asset/me_asset_index.h"
#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"

ME_DEFINE_LOG_CATEGORY(Asset);


MEEVENT_DECLARE_STATIC(registerAssetLoader);

static bool MEASSET_DEBUG_SINGLETHREADED_LOAD = 0;
constexpr u32 NUM_ASSET_COMPILER_THREADS = 1;

MEAPI meAssetSystem& meAssetSystemGet()
{
	return *GetEngineCtx()->assetSystem;
}

StringView meAssetTypeToString(meAssetType type)
{
	switch (type)
	{
		#define X(type, ext) case type: return STRING_LIT(#type);
		ME_DECLARE_ASSET_TYPES
		#undef X
		default: return STRING_LIT("UnknownAssetType");
	}
}

StringView meAssetFileExtFromType(meAssetType type)
{
    switch (type)
    {
        #define X(name, ext) case name: return STRING_LIT(ext);
        ME_DECLARE_ASSET_TYPES
        #undef X
        default: return {};
    }
}

ScopedAssetOpaqueLockR::ScopedAssetOpaqueLockR(const meAsset& asset)
{
	Init(asset);
}

meAssetLoader* ScopedAssetOpaqueLockR::Loader() const
{
	meAssetType type = asset.id.GetType();
	return meAssetTypeIsValid(type) ? meAssetSystemGet().assetLoaders[type] : nullptr;
}

void ScopedAssetOpaqueLockR::Init(const meAsset& sourceAsset)
{
	asset = sourceAsset;
	if (asset.isLoaded() || !asset.id)
	{
		InitLoadedRuntime();
		return;
	}
	InitTemplateAsset();
}

void ScopedAssetOpaqueLockR::InitTemplateAsset()
{
	ME_PROFILE_FUNCTION();
	MAID maid = asset.id;
	if (!maid || !meAssetTypeIsValid(maid.GetType()))
	{
		return;
	}

	meAssetRequestLoadTemplate(&maid, 1);
	if (!meAssetWaitUntilLoadstage({ &maid, 1 }, Loaded))
	{
		return;
	}

    if (meAsset* templateAsset = meAssetTryGetTemplate(maid))
    {
        asset = *templateAsset;
    }

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		if (!asset.runtimeContent)
		{
			asset.runtimeContent = loader->resourcePool->GetShared(asset.runtimeHandle);
		}
		resource = asset.runtimeContent;
	}
}

void ScopedAssetOpaqueLockR::InitLoadedRuntime()
{
	meAssetType type = asset.id.GetType();
	if (!meAssetTypeIsValid(type))
	{
		return;
	}

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		Eye handle = asset.isLoaded() ? asset.runtimeHandle : EYE_INVALID;
		if (!asset.runtimeContent)
		{
			asset.runtimeContent = loader->resourcePool->GetShared(handle);
		}
		resource = asset.runtimeContent;
	}
}

ScopedAssetOpaqueLockW::ScopedAssetOpaqueLockW(const meAsset& asset)
{
	Init(asset);
}

meAssetLoader* ScopedAssetOpaqueLockW::Loader() const
{
	meAssetType type = asset.id.GetType();
	return meAssetTypeIsValid(type) ? meAssetSystemGet().assetLoaders[type] : nullptr;
}

void ScopedAssetOpaqueLockW::Init(const meAsset& sourceAsset)
{
	asset = sourceAsset;
	if (asset.isLoaded() || !asset.id)
	{
		InitLoadedRuntime();
		return;
	}
	InitTemplateAsset();
}

void ScopedAssetOpaqueLockW::InitTemplateAsset()
{
	ME_PROFILE_FUNCTION();
	MAID maid = asset.id;
	if (!maid || !meAssetTypeIsValid(maid.GetType()))
	{
		return;
	}

	meAssetRequestLoadTemplate(&maid, 1);
	if (!meAssetWaitUntilLoadstage({ &maid, 1 }, Loaded))
	{
		return;
	}

    if (meAsset* templateAsset = meAssetTryGetTemplate(maid))
    {
        asset = *templateAsset;
    }

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		if (!asset.runtimeContent)
		{
			asset.runtimeContent = loader->resourcePool->GetShared(asset.runtimeHandle);
		}
		resource = asset.runtimeContent;
	}
}

void ScopedAssetOpaqueLockW::InitLoadedRuntime()
{
	meAssetType type = asset.id.GetType();
	if (!meAssetTypeIsValid(type))
	{
		return;
	}

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		Eye handle = asset.isLoaded() ? asset.runtimeHandle : EYE_INVALID;
		if (!asset.runtimeContent)
		{
			asset.runtimeContent = loader->resourcePool->GetShared(handle);
		}
		resource = asset.runtimeContent;
	}
}

String meAssetGetProjectRootResourceDir(EngineContext* engine)
{
	StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
	if (userAppConfigFile)
	{
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
		StringView userAppConfigDir = msFsGetDirFromPath(userAppConfigPathAbs);
		return userAppConfigDir;
	}
	return {};
}

void meAssetInitialize(EngineContext* engine)
{
	ME_PROFILE_FUNCTION();
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
	for (u32 i = 0; i < NUM_ASSET_TYPES; i++)
	{
		engine->assetSystem->registries[i].assets.reserve(50);
	}
    const CommandLineArgs& cmdline = GetCommandLineArgs();
	if (cmdline.hasResourceDir)
	{
		StringView cmdlineResDir = StringView(cmdline.ResourceDir, CStringLength(cmdline.ResourceDir));
		meAssetSetResourceDir(cmdlineResDir);
	}
	else
	{
		String rootResDir = meAssetGetProjectRootResourceDir(engine);
		meAssetSetResourceDir(rootResDir ? StringView(rootResDir) : meOSGetWorkingDir());
	}
	engine->assetSystem->assetCompilerJobs.Initialize(&engine->engineArena, NUM_ASSET_COMPILER_THREADS);
}

void meAssetInitializeLate(EngineContext* engine)
{
	ME_PROFILE_FUNCTION();
	registerAssetLoaderGet()(meEventPayload{ &engine->engineArena });
}

void meAssetTeardown(EngineContext* engine)
{
	engine->assetSystem->assetCompilerJobs.Shutdown();
    MEDELETE(&engine->engineArena, meAssetSystem, engine->assetSystem);
}

void meAssetRegisterLoader(meAssetLoader* loader)
{
	ME_PROFILE_FUNCTION();
    meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetType type = loader->assetType;
	ME_ASSERT(assetSystem.assetLoaders[type] == nullptr && "Not allowed to overwrite existing asset loader type");
	ME_ASSERT(type != MABadData);
	ME_ASSERT(loader->resourcePool != nullptr);
	ME_ASSERT(loader->assetTypeDesc != nullptr);
    // any "asset" must have a serialized header with its own asset id
    // if you hit this assert, make sure your asset struct inherits from meBaseAsset
	ME_ASSERT(loader->assetTypeDesc->fields[0].thisType == &TD_MESERIALIZEDHEADER);
	assetSystem.assetLoaders[type] = loader;
}

MAID sCreateNewAssetID(meAssetType type)
{
	// TODO: this shouldn't be random. 
    // for template assets, it should be a unique id across all template assets ever created
    //          Base it on an incrementing int in the asset index?
    // for instance assets, it can be unique just for this run of the engine
	f64 time = GetTimeUsec();
	u32 randomNumber = HashBytes((u8*)&time, sizeof(time));
	MAID newMaid = MAID(randomNumber, type);
	return newMaid;
}

meAsset meAssetCreateNewAsset(
    meAssetType type, 
    meResourceType resourceType,
    StringView templateFilename)
{
	ME_PROFILE_FUNCTION();
    MAID newMaid = sCreateNewAssetID(type);
    meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetLoader* loader = assetSystem.assetLoaders[type];
	ME_ASSERT(loader);
	meResourcePoolBase* resourcePool = loader->resourcePool;

	Eye newRuntimeResource = resourcePool->Load({.resourceType = resourceType});
	meSharedPtr<void> assetContent = resourcePool->GetShared(newRuntimeResource);
	void* opaqueAssetData = assetContent.get();
	meSerializedHeader* assetHeader = (meSerializedHeader*)opaqueAssetData;
	assetHeader->assetHeader = newMaid;

	meAsset newAsset = meAsset(newRuntimeResource, newMaid);
	newAsset.runtimeContent = meMove(assetContent);
    if (templateFilename)
    {
        meFsNormalizePathSeperators(templateFilename);
        templateFilename = meAssetEnsurePathHasGoodExtension(templateFilename, type);
        templateFilename = meAssetGetRelPathForResource(templateFilename);
        meAssetIndexRegisterRelation(templateFilename, newAsset.id);
    }

    // NOTE: currently only template assets are tracked by the global asset registry
    // instance ones are kept alive by their references in the resource pools/callers
	if (resourceType == meResourceType_TemplateAsset)
	{
		meAssetTypeRegistry& reg = assetSystem.registries[type];
		RWLockWrite lock(reg.lock);
		reg.assets[newMaid] = newAsset;
	}
	return newAsset;
}

MAID::MAID(u64 id, meAssetType type)
{
    SetID(id);
    SetType(type);
}

meAssetLoadStage meAssetLoader::meAssetWaitForLoadstage(
    meSpanTyped<MAID> assets, 
    meAssetLoadStage loadStage)
{
	ME_PROFILE_FUNCTION();
    for (u32 i = 0; i < assets.size; i++)
    {
        MAID maid = assets[i];
        meAsset* asset = meAssetTryGetTemplate(maid);
        constexpr u32 maxAttempts = 1000;
        u32 attempts = 0;
        while (asset && asset->loadStage != loadStage && attempts++ < maxAttempts)
        {
            meThreadSleep(1); // TMP
            //GetAssetSystem().assetCompilerJobs.WaitOnJob(assetJobId);
            asset = meAssetTryGetTemplate(maid);
        }
        if (!asset || asset->loadStage != loadStage)
        {
            return Unloaded;
        }
    }
    return loadStage;
}

meAssetLoadStage meAssetLoader::meAssetWaitForLoadstage(
	const MAID& maid,
	meAssetLoadStage loadStage)
{
	ME_PROFILE_FUNCTION();
    return meAssetWaitForLoadstage({&maid, 1}, loadStage);
}

meJobId meAssetRequestLoadTemplate(
	MAID* assetIdents, 
	u32 numAssets,
    meAssetOnAssetLoadCb cb)
{
	ME_PROFILE_FUNCTION();
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < numAssets; i++)
	{
		const MAID& assetIdent = assetIdents[i];
		ME_ASSERT(assetIdent);
		meAssetType assetType = meAssetType(assetIdent.GetType());
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (!loader)
		{
			LOG_ERROR(Asset, "Tried to load asset type that doesn't have an implemented loader");
			return {}; // dev error, should never happen, unrecoverable
		}
		meAsset* asset = meAssetTryGetTemplate(assetIdent);
		// if it's already loaded, noop
		if (asset)
		{
			stage = asset->loadStage;
		}
		if (stage == Unloaded)
		{
			{ // add the slot in, and mark it as "loading"
				meAsset notYetLoadedData = meAsset(assetIdent, Loading);
				meAssetTypeRegistry& reg = assetSystem.registries[assetType];
				RWLockWrite lock(reg.lock);
				reg.assets[assetIdent] = notYetLoadedData;
			}
			struct AssetCompilerJobData
			{
				MAID ident;
				meAssetLoader* loader;
				meAssetOnAssetLoadCb cb;
			};
			assetSystem.assetBeginLoadingEvent(meEventPayload((void*)&assetIdent));
			AssetCompilerJobData jobData = {};
			jobData.ident = assetIdent;
			jobData.loader = loader;
			jobData.cb = cb;
			auto fn = [jobData, &assetSystem]() 
			{
				ME_PROFILE_FUNCTION();
				meAsset* asset = meAssetTryGetTemplate(jobData.ident);
				ME_ASSERT(asset);
				jobData.loader->meAssetLoad(*asset);
				ME_ASSERT(asset->id);
				if (!asset->isLoaded())
				{
					return;
				}
				// each asset type can respond to loaded events
				jobData.loader->meAssetOnLoad(*asset);
				// globally, systems can also respond
				assetSystem.assetFinishedLoadingEvent(meEventPayload((void*)&jobData.ident));
                // and this individual request can also respond
				if (jobData.cb)
				{
					jobData.cb(*asset);
				}
			};
			if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
			{
				fn();
			}
			else
			{
				meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
                // TODO: support multiple async jobs that are all grouped into a single "task group" id to return here.
                ME_ASSERT(numAssets == 0);
				return compilerJobId;
			}
		}
        else if (stage == Loaded)
        {
            // if the asset was already loaded, still call the cb
            if (cb)
            {
                cb(*asset);
            }
        }
	}
	return {};
}

bool meAssetWaitUntilLoadstage(
	meSpanTyped<MAID> assetIdents, 
	meAssetLoadStage loadStage)
{
	ME_PROFILE_FUNCTION();
    for (u32 i = 0; i < assetIdents.size; i++)
	{
		// dispatch to the loader for this asset type
		const MAID& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdents[i].GetType());
		const meAssetSystem& assetSystem = meAssetSystemGet();
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		if (loader)
		{
			meAssetLoadStage loadedStage = loader->meAssetWaitForLoadstage(assetIdent, loadStage);
			if (loadedStage != loadStage)
			{
				return false;
			}
		}
	}
	return true;
}

meJobId meAssetRequestWriteTemplate(
	meSpanTyped<MAID> assetIdents,
	meAssetOnAssetLoadCb onWriteCb)
{
	ME_PROFILE_FUNCTION();
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < assetIdents.size; i++)
	{
		const MAID& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdent.GetType());
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (!loader)
		{
			LOG_ERROR(Asset, "Tried to write asset type that doesn't have an implemented loader");
			return {}; // engine dev error, should never happen
		}
		meAsset* asset = meAssetTryGetTemplate(assetIdent);
		// if it's already loaded, noop
		if (asset)
		{
			stage = asset->loadStage;
		}
		if (stage == Loaded)
		{
			struct AssetCompilerJobData
			{
				MAID ident;
				meAssetLoader* loader;
				meAssetOnAssetLoadCb cb;
			};
			assetSystem.assetBeginLoadingEvent(meEventPayload((void*)&assetIdent));
			AssetCompilerJobData jobData = {};
			jobData.ident = assetIdent;
			jobData.loader = loader;
			jobData.cb = onWriteCb;
			auto fn = [jobData, &assetSystem]() 
			{
				ME_PROFILE_FUNCTION();
				meAsset* asset = meAssetTryGetTemplate(jobData.ident);
				ME_ASSERT(asset && asset->loadStage == Loaded && asset->runtimeHandle && asset->id);
				jobData.loader->meAssetWrite(*asset);
				if (jobData.cb)
				{
					jobData.cb(*asset);
				}
				assetSystem.assetFinishedWritingEvent(meEventPayload((void*)&jobData.ident));
			};
			if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
			{
				fn();
			}
			else
			{
                // TODO: support multiple async jobs that are all grouped into a single "task group" id to return here.
                ME_ASSERT(assetIdents.size == 0);
				meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
				return compilerJobId;
			}
		}
	}
	return {};
}

meJobId meAssetRequestImport(
    meSpanTyped<tuplet::tuple<OSFileReference, meAssetType>> files,
    meAssetOnImportCb onImportCb)
{
    ME_PROFILE_FUNCTION();
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < files.size; i++)
	{
        struct JobData
        {
            OSFileReference file;
            meAssetType type;
            meAssetOnImportCb cb;
        };
        JobData jobData = {};
        jobData.file = tuplet::get<0>(files[i]);
        jobData.type = tuplet::get<1>(files[i]);
        jobData.cb = onImportCb;
        auto fn = [jobData, &assetSystem]() 
        {
            ME_PROFILE_FUNCTION();
            meAssetLoader* loader = meAssetTypeIsValid(jobData.type) ? assetSystem.assetLoaders[jobData.type] : nullptr;
            bool success = loader && loader->meAssetImport(jobData.file);
            if (!success)
            {
                LOG_ERROR(Asset, "Failed to import {} as {}", LogSV(jobData.file.GetPath()), LogSV(meAssetTypeToString(jobData.type)));
            }
            if (jobData.cb)
            {
                jobData.cb(jobData.file, success);
            }
        };
        if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
        {
            fn();
        }
        else
        {
            meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
            // TODO: support multiple async jobs that are all grouped into a single "task group" id to return here.
            ME_ASSERT(files.size == 0);
            return compilerJobId;
        }
	}
	return {};
}

bool meAssetLoader::meAssetCompile(meAsset& asset)
{
    ME_PROFILE_FUNCTION();
    ME_ASSERT(asset.id.GetType() == assetType);
    meAsset* loaded = meAssetTryGetTemplate(asset.id);
    // if the asset already has some loaded data, that implies we've already compiled it before, so early out
    if (loaded && loaded->isLoaded())
    {
        if (&asset != loaded)
        {
            asset = *loaded;
        }
        return true;
    }
    asset.loadStage = Compiling;
    if (!asset.runtimeContent)
    {
        asset.runtimeHandle = resourcePool->Load({.resourceType = meResourceType_TemplateAsset});
        asset.runtimeContent = resourcePool->GetShared(asset.runtimeHandle);
    }

    StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
    StringView assetPath = meAssetGetAbsPathForResource(diskPath);
    meSerializeResult result;
    result.ownerMaid = asset.id;
    DeserializeContext ctx = {};
    ctx.mode = meSerializationMode_Text;
    ctx.typeDesc = assetTypeDesc;
    ctx.externalDataAllocator = resourcePool->GetPayloadAllocator();
    ctx.outputData = meSpan(asset.runtimeContent.get(), assetTypeDesc->size);
    ctx.outResult = &result;
    DeserializeFromFileBlocking(assetPath, ctx);
    if (!result)
    {
        return false;
    }

    meBaseAsset* content = asset.GetRuntimeContent().get();
    ME_ASSERT(content->header.assetHeader == asset.id);
    return meAssetPrepareCompiledData(asset);
}

void meAssetLoader::meAssetLoad(meAsset& asset)
{
    ME_PROFILE_FUNCTION();
    ME_ASSERT(asset.id.GetType() == assetType);
    // "loading" an asset:
    // FIRST, if the asset only exists as the source editor data + some loose files, we need to "compile" it
    // into a version ready to be used by the engine runtime. Each asset type can do specialized logic here.
    //  I.E. text -> binary & doing necessary prep operations EX: simplify/optimize mesh data, generate mips, versioning, etc.
    // NEXT we load the compiled asset data (potentially from a cache), then allow each asset type to do specialized logic
    // to prepare the loaded asset to be actually used ingame. EX: uploading data to the GPU
    bool success = meAssetCompile(asset);
    if (success && asset.isLoaded())
    {
        return;
    }
    if (success)
    {
        asset.loadStage = Loading;
        success = meAssetInitializeRuntime(asset);
    }
    if (!success)
    {
        LOG_WARN(Asset, "Failed to compile or initialize asset {}", LogSV(meAssetIndexGetFilesystemPath(asset.id)));
        Eye failedHandle = asset.runtimeHandle;
        asset.runtimeContent.reset();
        asset.runtimeHandle = EYE_INVALID;
        asset.loadStage = Unloaded;
        if (failedHandle != EYE_INVALID)
        {
            resourcePool->Destroy(failedHandle);
        }
        return;
    }
    // a cheeky assumption. If you're loading this asset, you will almost certainly end up loading it's dependencies very soon.
    meSpanTyped<MAID> deps = meAssetIndexGetDependencies(asset.id);
    if (deps.isValid())
    {
        meAssetRequestLoadTemplate(deps, (u32)deps.size);
    }
    asset.loadStage = Loaded;
}

void meAssetUnloadBlocking(meSpanTyped<meAsset> assets)
{
	ME_PROFILE_FUNCTION();
	meAssetSystem& assetSystem = meAssetSystemGet();
    for (u32 i = 0; i < assets.size; i++)
    {
        meAsset& asset = assets[i];
		Eye handle = asset.runtimeHandle;
        // modify registry first, so we don't have assets in the registry with invalid data
        if (asset.IsTemplateAsset())
        {
            if (meAsset* registryAsset = meAssetTryGetTemplate(asset.id))
            {
                RWLockWrite lock(assetSystem.registries[asset.id.GetType()].lock);
                registryAsset->loadStage = Unloaded;
                registryAsset->runtimeContent.reset();
                registryAsset->runtimeHandle = EYE_INVALID;
            }
        }
		asset.runtimeContent.reset();
		asset.runtimeHandle = EYE_INVALID;
		asset.loadStage = Unloaded;
		if (handle != EYE_INVALID && meAssetTypeIsValid(asset.id.GetType()))
		{
			meAssetLoader* loader = assetSystem.assetLoaders[asset.id.GetType()];
			ME_ASSERT(loader);
			loader->resourcePool->Destroy(handle);
		}
    }
}

bool meAssetLoader::meAssetWrite(meAsset& asset)
{
	ME_PROFILE_FUNCTION();
	meResourcePoolBase* pool = resourcePool;
	if (!asset.isLoaded())
	{
		LOG_WARN(Asset, "Attempted to write an unloaded asset");
		return false;
	}
	ME_ASSERT(assetTypeDesc->fields[0].thisType == &TD_MESERIALIZEDHEADER);

	meSharedPtr<void> content = asset.runtimeContent;
	if (!content)
	{
		content = pool->GetShared(asset.runtimeHandle);
	}
	void* assetData = content.get();
	StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
	StringView assetPath = meAssetGetAbsPathForResource(diskPath);
	meAllocator* tempAllocator = GetTLScratch();
	SerializeContext ctx = {};
	ctx.mode = meSerializationMode_Text;
	ctx.typeDesc = assetTypeDesc;
	ctx.allocator = tempAllocator;
	ctx.sourceData = meSpan(assetData, assetTypeDesc->size);
	meSerializeResult res = SerializeBlocking(ctx);
	if (res != meSerializeResult::SER_SUCCESS)
	{
		LOG_ERROR(Asset, "Failed to serialize asset {}", LogSV(assetPath));
		return false;
	}
	StringView assetSerializedString(ctx.serializedData);
	if (!meOSWriteFileContentAtomic(assetPath, assetSerializedString.data, assetSerializedString.len))
	{
		LOG_ERROR(Asset, "Failed to write asset to file. filename = {}\nassetString = {}", LogSV(assetPath), LogSV(assetSerializedString));
		return false;
	}
	return true;
}

meAsset* meAssetTryGetTemplate(MAID assetID)
{
    if (!assetID)
    {
        return nullptr;
    }
	meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetTypeRegistry& reg = assetSystem.registries[assetID.GetType()];
	RWLockRead lock(reg.lock);
	auto it = reg.assets.find(assetID);
	if (it == reg.assets.end())
	{
		return nullptr;
	}
	return &it->second;
}

void meAssetSetResourceDir(StringView dir)
{
    GetEngineCtx()->assetSystem->resourceDir = dir;
	LOG_INFO(Asset, "Set resource dir = {}", LogSV(dir));
}

StringView meAssetGetResourceDir()
{
    return GetEngineCtx()->assetSystem->resourceDir;
}

StringView meAssetGetAbsPathForResource(StringView resourcePath)
{
	if (!resourcePath)
	{
		return {};
	}
	meFsNormalizePathSeperators(resourcePath);
	StringView result = resourcePath;
	if (FindInString(resourcePath, meAssetGetResourceDir()) == -1)
	{
		StringView resDir = meAssetGetResourceDir();
		result = StringFormatTmp("%.*s%.*s%.*s", STRING_VAARGS(resDir), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(resourcePath));
	}
	return result;
}

StringView meAssetGetRelPathForResource(StringView resourcePath)
{
	StringView resDir = meAssetGetResourceDir();
	s32 idx = FindInString(resourcePath, resDir);
	if (idx != -1)
	{
		StringView cropped = resourcePath.OffsetView(resDir.len);
		cropped = EatChars(cropped, meFsGetDirectorySeperator());
		return cropped;
	}
	return resourcePath;
}

meSpan meSerializeTryGetSerializedHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer)
{
	meSpan result = {};
	meTypeDescriptorWalkMembers(typeDesc, serializedBuffer.data,
		[&](const meTypeDescriptorMember& member)
		{
			// search top-level fields for the meBaseAsset serialized header
			if (member.field.thisType == &TD_MESERIALIZEDHEADER && StringCompare(member.field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME)))
			{
				result = serializedBuffer.Subspan(member.offsetBytes, member.field.size);
				return false;
			}
			return true;
		},
		false);
	return result;
}

meSpan meSerializeTryGetAssetHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer)
{
	meSpan serializedHeader = meSerializeTryGetSerializedHeader(typeDesc, serializedBuffer);
	if (serializedHeader)
	{
		return serializedHeader.Subspan(offsetof(meSerializedHeader, assetHeader), sizeof(MAID));
	}
	return {};
}


StringView meAssetEnsurePathHasGoodExtension(
    const StringView& assetPath, 
    meAssetType inputType)
{
    meAssetType guessedType = meAssetFindAssetTypeFromFilepath(assetPath);
    if (guessedType != MABadData)
    {
        return assetPath;
    }
    // if we can't figure out the asset type from the filepath
    // chop off the extension and add the proper one to the end
    s32 firstDot = FindInString(assetPath, STRING_LIT("."));
    StringView noExtStr = firstDot != -1 ? assetPath.OffsetView(firstDot) : assetPath;
    return StringFormatTmp(STRING_FMT "." STRING_FMT ME_ASSET_EXTENSION, 
        STRING_VAARGS(noExtStr), STRING_VAARGS(meAssetFileExtFromType(inputType)));
}
