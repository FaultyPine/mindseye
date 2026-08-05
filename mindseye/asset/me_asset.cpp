#include "me_asset.h"

#include "asset/me_asset_index.h"
#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"
#include "core/me_profile.h"


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

	meAsset* registeredAsset = meAssetTryGetTemplate(maid);
	if (registeredAsset && registeredAsset->isLoaded())
	{
		asset = *registeredAsset;
	}
	else
	{
		meAssetRequestLoadTemplate(&maid, 1);
		if (!meAssetWaitUntilLoadstage({ &maid, 1 }, Loaded))
		{
			return;
		}

		meAssetSystem& sys = meAssetSystemGet();
		meAssetTypeRegistry& reg = sys.registries[maid.GetType()];
		{
			RWLockRead lock(reg.lock);
			auto it = reg.assets.find(maid);
			if (it == reg.assets.end() || !it->second.isLoaded())
			{
				return;
			}
			asset = it->second;
		}
	}

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		resource = loader->resourcePool->GetOpaque(asset.runtimeHandle);
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
		resource = loader->resourcePool->GetOpaque(handle);
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

	meAssetSystem& sys = meAssetSystemGet();
	meAssetTypeRegistry& reg = sys.registries[maid.GetType()];
	{
		RWLockRead lock(reg.lock);
		auto it = reg.assets.find(maid);
		if (it == reg.assets.end() || !it->second.isLoaded())
		{
			return;
		}
		asset = it->second;
	}

	meAssetLoader* loader = Loader();
	if (loader && loader->resourcePool)
	{
		resource = loader->resourcePool->GetOpaque(asset.runtimeHandle);
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
		resource = loader->resourcePool->GetOpaque(handle);
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
	registerAssetLoader( meEventPayload{ &engine->engineArena });
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
	// TODO: this shouldn't be random. Base it on an incrementing int in the asset index
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
	void* opaqueAssetData = resourcePool->GetOpaque(newRuntimeResource);
	meSerializedHeader* assetHeader = (meSerializedHeader*)opaqueAssetData;
	assetHeader->assetHeader = newMaid;

	meAsset newAsset = meAsset(newRuntimeResource, newMaid);
    if (templateFilename)
    {
        meFsNormalizePathSeperators(templateFilename);
        templateFilename = meAssetEnsurePathHasGoodExtension(templateFilename, type);
        templateFilename = meAssetGetRelPathForResource(templateFilename);
        meAssetIndexRegisterRelation(templateFilename, newAsset.id);
    }

    meAssetTypeRegistry& reg = assetSystem.registries[type];
    RWLockWrite lock(reg.lock);
    reg.assets[newMaid] = newAsset; // copy
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
        if (asset->loadStage != Loaded)
        {
            return Unloaded;
        }
    }
    return Loaded;
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
			LOG_ERROR("Tried to load asset type that doesn't have an implemented loader");
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
				ME_ASSERT(asset->loadStage == Loaded && asset->runtimeHandle);
				// each asset type can respond to loaded events
				jobData.loader->meAssetOnLoad(*asset);
				// globally, systems can also respond
				assetSystem.assetFinishedLoadingEvent(meEventPayload((void*)&jobData.ident));
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
			LOG_ERROR("Tried to write asset type that doesn't have an implemented loader");
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
				meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
				return compilerJobId;
			}
		}
	}
	return {};
}

void meAssetLoader::meAssetLoad(meAsset& asset)
{
	ME_PROFILE_FUNCTION();
	ME_ASSERT(asset.id.GetType() == assetType);
	meAllocator* allocator = resourcePool->GetPayloadAllocator();
	// TODO: implement async loading, so this would return loadStage=Loading
	// and would itself enqueue more asset compiling jobs for the individual parts of the asset
	meResourcePoolBase* pool = resourcePool;
	asset.runtimeHandle = pool->Load({.resourceType = meResourceType_TemplateAsset});
	void* outAsset = pool->GetOpaque(asset.runtimeHandle);
	StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
	StringView assetPath = meAssetGetAbsPathForResource(diskPath);
	// Set ownerMaid before deserializing so that meAssetDeserializerFn
	// registers any referenced asset MAIDs in the asset index under this asset's key.
	meSerializeResult result;
	result.ownerMaid = asset.id;
	DeserializeContext ctx = {};
	ctx.mode = meSerializationMode_Text;
	ctx.typeDesc = assetTypeDesc;
	ctx.externalDataAllocator = allocator;
	ctx.outputData = meSpan(outAsset, assetTypeDesc->size);
	ctx.outResult = &result;
	DeserializeFromFileBlocking(assetPath, ctx);
	if (result)
	{
		ME_ASSERT(assetTypeDesc->fields[0].thisType == &TD_MESERIALIZEDHEADER);
		meSerializedHeader* header = (meSerializedHeader*)outAsset;
		ME_ASSERT(header->assetHeader == asset.id);
	}
	else
	{
		LOG_WARN("Failed to load asset " STRING_FMT, STRING_VAARGS(diskPath));
	}
	// we'll almost certainly need the (template) deps for this asset if we're loading it, so just start those now
	meSpanTyped<MAID> deps = meAssetIndexGetDependencies(asset.id);
    if (deps)
    {
        meAssetRequestLoadTemplate(deps, (u32)deps.size);
    }
	
	asset.loadStage = result ? Loaded : Unloaded;
}

void meAssetUnloadBlocking(meSpanTyped<meAsset> assets)
{
	ME_PROFILE_FUNCTION();
	meAssetSystem& assetSystem = meAssetSystemGet();
    for (u32 i = 0; i < assets.size; i++)
    {
        meAsset& asset = assets[i];
        // modify registry first, so we don't have assets in the registry with invalid data
        if (asset.IsTemplateAsset())
        {
            if (meAsset* registryAsset = meAssetTryGetTemplate(asset.id))
            {
                RWLockWrite lock(assetSystem.registries[asset.id.GetType()].lock);
                registryAsset->loadStage = Unloaded;
                registryAsset->runtimeHandle = {};
            }
        }
        meAssetLoader* loader = assetSystem.assetLoaders[asset.id.GetType()];
        ME_ASSERT(loader);
        loader->resourcePool->Destroy(asset.runtimeHandle);
    }
}

void meAssetLoader::meAssetWrite(meAsset& asset)
{
	ME_PROFILE_FUNCTION();
	meResourcePoolBase* pool = resourcePool;
	if (!asset.isLoaded())
	{
		LOG_WARN("Attempted to write an unloaded asset");
		return;
	}
	ME_ASSERT(assetTypeDesc->fields[0].thisType == &TD_MESERIALIZEDHEADER);

	void* assetData = pool->GetOpaque(asset.runtimeHandle);
	StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
	StringView assetPath = meAssetGetAbsPathForResource(diskPath);
	meAllocator* tempAllocator = GetTLScratch();
	SerializeContext ctx = {};
	ctx.mode = meSerializationMode_Text;
	ctx.typeDesc = assetTypeDesc;
	ctx.allocator = tempAllocator;
	ctx.sourceData = meSpan(assetData, assetTypeDesc->size);
	meSerializeResult res = SerializeBlocking(ctx);
	ME_ASSERT(res == meSerializeResult::SER_SUCCESS);
	StringView assetSerializedString((char*)ctx.serializedData.data, ctx.serializedData.size);
	// TODO: split the actual writing out into a separate thing?
	OSFileReference file;
	meOSOpenFile(file, assetPath, (OSFileFlags_StompExisting | OSFileFlags_ScopedFile));
	if (!meOSWriteFileContent(file, assetSerializedString.data, assetSerializedString.len))
	{
		LOG_ERROR("Failed to write asset to file. filename = " STRING_FMT "\nassetString = " STRING_FMT, STRING_VAARGS(assetPath), STRING_VAARGS(assetSerializedString));
	}
}

meAsset* meAssetTryGetTemplate(MAID assetID)
{
    if (!assetID)
    {
        return nullptr;
    }
	meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetTypeRegistry& reg = assetSystem.registries[assetID.GetType()];
	RWLockRead(reg.lock);
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
	LOG_INFO("Set resource dir = " STRING_FMT, STRING_VAARGS(dir));
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

