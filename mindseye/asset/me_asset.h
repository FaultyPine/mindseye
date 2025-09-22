#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "core/thread/me_rw_lock.h"
#include "core/me_job_system.h"

#define ME_DECLARE_ASSET_TYPES \
X(BadData)\
X(Scene)\
X(Shader)\
X(Image)

enum meAssetType : u8
{
#define X(name) name,
ME_DECLARE_ASSET_TYPES
#undef X
    NUM_ASSET_TYPES
};
#define ASSET_TYPE_BITS (((u64)0xff) << 48)
STATIC_ASSERT(NUM_ASSET_TYPES < 128);


// mindseye asset id
struct MAID
{
    u64 id: 48;
    u64 type: 8;
    bool isValid() const { return id != 0; }
    bool operator==(const MAID& other) const { return id == other.id && type == other.type; }
    operator u64() const { return ((u64)id) | ((u64)type << 48); }
    operator meAssetType() const { return meAssetType(type); }
};
MEMAP_BEGIN_CUSTOM_HASHER(MAID, obj) 
{
    size_t h1 = std::hash<int>{}(obj.id);
    size_t h2 = std::hash<int>{}(obj.type);
    return h1 ^ (h2 << 1);
} MEMAP_END_CUSTOM_HASHER

STATIC_ASSERT(sizeof(MAID) == sizeof(u64));
constexpr MAID MAID_INVALID = {.id = 0, .type = 0};


enum meAssetLoadStage
{
    Unloaded = 0, 
	Loading, 
	Loaded, 
	Unloading, 
	
	LoadStageCount
};

// "runtime" asset data
struct meRTAsset
{
    MAID id = MAID_INVALID;
    meOwningSpan loadedData = {};
    meAssetType type = BadData;
    meAssetLoadStage loadStage = Unloaded;
	bool isLoaded() const 
	{
		return id != MAID_INVALID && loadedData.isValid() && 
			   type != BadData    && loadStage == Loaded; 
	}
};

// identifies an asset "on disk".
// these can map to filesystem paths, or something else if assets are being loaded/fetched from some other mechanism
struct meAssetIdent
{
    MAID id = MAID_INVALID;
    // TODO: will be a hash of the "source" data that the asset is created from.
    // EX: shaders will be a hash of their source file. Images - hash of the .png or whatever
    u32 assetSourceHash = 0;
    // TODO: version of the type of this asset. 
    // I.E. "shader version 0" -> "shader version 1" (maybe i switch shader compilers, or change some option in the compiler flags...)
    u32 assetVersion = 0;
    bool operator==(const meAssetIdent& other) const 
	{ 
		return id == other.id && assetSourceHash == other.assetSourceHash && assetVersion == other.assetVersion; 
	}
};

MEMAP_BEGIN_CUSTOM_HASHER(meAssetIdent, ident) 
{
    size_t h1 = std::hash<MAID>{}(ident.id);
    size_t h2 = std::hash<u32>{}(ident.assetSourceHash);
    size_t h3 = std::hash<u32>{}(ident.assetVersion);
	return HashCombine(h1, h2, h3);
} MEMAP_END_CUSTOM_HASHER

struct meAssetLoader
{
    virtual meRTAsset meAssetLoad(meAssetIdent) = 0;
	virtual meAssetLoadStage meAssetWaitForLoad(meAssetIdent);
};

struct meAssetSystem
{
    // relative to working dir
    const char* resourceDir = nullptr;
	RWLock assetRegistryLock = {};
    meMap<meAssetIdent, meRTAsset> assetRegistry = {};
    // meAssetType -> loader
    meAssetLoader* assetLoaders[NUM_ASSET_TYPES] = {};
	meJobSystem assetCompilerJobs = {};
};

void meAssetInitialize(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);
void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type);

meAssetLoadStage* meAssetRequestLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1);

meAssetLoadStage* meAssetWaitForLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1);

meRTAsset* meAssetTryGetLoaded(meAssetIdent asset);

meAssetLoadStage* meAssetLoadSync(
	meAllocator* allocator,
	meAssetIdent* idents,
	u32 numAssets = 1);

void meAssetSetResourceDir(const char* dir);
const char* meAssetGetResourceDir();
// I.E. "models/obj.gltf" -> "C:/workingdir/mindseye/bin/resource/models/obj.gltf" or something similar
// returns a short-lived string. This should only be used for "scratch" operations. If you need to store this string long-term,
// copy it, or use something else
const char* meAssetResource(StringView resourcePath);

