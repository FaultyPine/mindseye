#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "core/thread/me_rw_lock.h"
#include "core/me_job_system.h"
#include "core/me_event.h"

#include "generatedtypes/me_asset.generated.h"

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
STATIC_ASSERT(NUM_ASSET_TYPES < 255);


// Mindseye Asset ID
struct MEREFLECT(type) MAID
{
	constexpr static u32 ID_BITS = 48; // lower bits
	constexpr static u32 TYPE_BITS = 8; // top bits
    MAID() = default;
    MAID(u64 id, meAssetType type);
	u64 idAndType = U32_INVALID_ID;
    bool isValid() const { return idAndType != U32_INVALID_ID; }
    bool operator==(const MAID& other) const { return idAndType == other.idAndType; }
	inline u64 GetType() const
	{
		return idAndType >> ID_BITS;
	}
    inline void SetType(meAssetType type)
    {
        u64 typefull = (u64)type;
        typefull = typefull << ID_BITS;
        idAndType |= typefull;
    }
	inline u64 GetID() const
	{
		return idAndType & (~0 >> TYPE_BITS);
	}
    inline void SetID(u64 id)
    {
        // make sure top type bits aren't set
        ME_ASSERT(id == (id & ~(((u64)0xff) << ID_BITS)));
        idAndType |= id;
    }
    operator u64() const { return idAndType; }
};
MEMAP_BEGIN_CUSTOM_HASHER(MAID, obj) 
{
    size_t h1 = std::hash<int>{}(obj.idAndType);
	return h1;
} MEMAP_END_CUSTOM_HASHER

STATIC_ASSERT(sizeof(MAID) == sizeof(u64));
constexpr MAID MAID_INVALID = {};


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
    String diskIdent = {};
    meAssetIdent() = default;
    meAssetIdent(StringView diskIdent, meAssetType type);
    bool operator==(const meAssetIdent& other) const 
	{ 
		return id == other.id && assetSourceHash == other.assetSourceHash; 
	}
};

MEMAP_BEGIN_CUSTOM_HASHER(meAssetIdent, ident) 
{
    size_t h1 = std::hash<MAID>{}(ident.id);
    size_t h2 = std::hash<u32>{}(ident.assetSourceHash);
	return HashCombine(h1, h2);
} MEMAP_END_CUSTOM_HASHER

struct meAssetLoader
{
    // called on asset threads
    virtual meRTAsset meAssetLoad(meAssetIdent) = 0;
	meAssetLoadStage meAssetWaitForLoad(meAssetIdent);
};

struct meAssetSystem
{
	// relative to working dir
	String resourceDir = {};
	RWLock assetRegistryLock = {};
    meMap<meAssetIdent, meRTAsset> assetRegistry = {};
    // meAssetType -> loader
    meAssetLoader* assetLoaders[NUM_ASSET_TYPES] = {};
	meJobSystem assetCompilerJobs = {}; // TODO: replace this with a unified job system which should have multiple "queue" types
    meEvent assetBeginLoadingEvent = {};
    meEvent assetFinishedLoadingEvent = {};
};

void meAssetInitialize(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);
void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type);

typedef void(*meAssetOnAssetLoadCb)(const meRTAsset&);

meAssetLoadStage* meAssetRequestLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1,
    meAssetOnAssetLoadCb cb = nullptr);

meAssetLoadStage* meAssetWaitForLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1);

meRTAsset* meAssetTryGetLoaded(meAssetIdent asset);

meAssetLoadStage* meAssetLoadSync(
	meAllocator* allocator,
	meAssetIdent* idents,
	u32 numAssets = 1);

void meAssetSetResourceDir(StringView dir);
StringView meAssetGetResourceDir();
// I.E. "models/obj.gltf" -> "C:/workingdir/mindseye/bin/resource/models/obj.gltf" or something similar
// returns a short-lived string. This should only be used for "scratch" operations. If you need to store this string long-term,
// copy it, or use something else
// NOTE: allocates a temporary buffer
StringView meAssetResource(StringView resourcePath);
