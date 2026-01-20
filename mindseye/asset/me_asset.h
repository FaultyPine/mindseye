#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "core/thread/me_rw_lock.h"
#include "core/me_job_system.h"
#include "core/me_event.h"

#include "generatedtypes/me_asset.generated.h"

#define ME_DECLARE_ASSET_TYPES \
X(MABadData)\
X(MAScene)\
X(MAShader)\
X(MAMaterial)\
X(MAMesh)\
X(MATexture)

enum meAssetType : u8
{
#define X(name) name,
ME_DECLARE_ASSET_TYPES
#undef X
    NUM_ASSET_TYPES
};
STATIC_ASSERT(NUM_ASSET_TYPES < 255);
constexpr static u32 MAID_ID_BITS = 48; // lower bits
constexpr static u32 MAID_TYPE_BITS = 8; // top bits

// Mindseye Asset ID
struct MEREFLECT(type) MAID
{
	u64 idAndType = U32_INVALID_ID;

    MAID() = default;
    MAID(u64 id, meAssetType type);
	template <meAssetType T>
	MAID(u64 id)
	{
		SetType(T);
		SetID(id);
	}
    bool isValid() const { return idAndType != U32_INVALID_ID; }
    bool operator==(const MAID& other) const { return idAndType == other.idAndType; }
	inline u64 GetType() const
	{
		return idAndType >> MAID_ID_BITS;
	}
    inline void SetType(meAssetType type)
    {
        u64 typefull = (u64)type;
        typefull = typefull << MAID_ID_BITS;
        idAndType |= typefull;
    }
	inline u64 GetID() const
	{
		return idAndType & (~0 >> MAID_TYPE_BITS);
	}
    inline void SetID(u64 id)
    {
        // make sure top type bits aren't set
        ME_ASSERT(id == (id & ~(((u64)0xff) << MAID_ID_BITS)));
		idAndType &= (~0ull << MAID_ID_BITS); // clear all id bits
        idAndType |= id; // set id bits
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

// identifies an asset "on disk".
// these can map to filesystem paths, or something else if assets are being loaded/fetched from some other mechanism
struct MEREFLECT(type) meAssetIdent
{
    MAID id = MAID_INVALID;
	MEREFLECT(exclude) String diskIdent = {};
	// TODO: hash the source when loading it from disk
	MEREFLECT(exclude) u32 assetSourceHash = 0; // including the hash in the serialized asset itself would mean a change to asset A requires updating all assets that depend on it, which we don't want
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

// storing both the "load-time" and "usage-time" information, this is meant to be
// both serialized, and also used for loading at runtime. This is what will be in the fields
// of asset definitions. I.E. when a "scene" asset references a "mesh" asset, use this structure
struct MEREFLECT(type) meAsset
{
	meAssetIdent asset = {};
	MEREFLECT(exclude)
	Eye runtimeHandle = {};
    meAssetLoadStage loadStage = Unloaded;
	
	meAsset(const meAssetIdent& ident, meAssetLoadStage stage) :
		asset(ident), loadStage(stage)
	{}
	// initialized with both "load-time" and "usage-time" info
	meAsset(Eye eye, const meAssetIdent& ident) : 
		asset(ident), runtimeHandle(eye) 
	{
		if (runtimeHandle)
		{
			loadStage = Loaded;
		}
	}
	// can be initialized as a usage-only concept. 
	// I.E. Generating meshes/etc on-the-fly without an associated on-disk asset.
	meAsset(Eye eye) : runtimeHandle(eye)
	{
		if (runtimeHandle)
		{
			loadStage = Loaded;
		}
	}
	meAsset() = default;

	bool isLoaded() const 
	{
		return runtimeHandle != EYE_INVALID && loadStage == Loaded; 
	}
	operator const Eye() const { return runtimeHandle; }
	operator const MAID() const { return asset.id; }
};

struct meAssetLoader
{
    // called on asset threads
    virtual void meAssetLoad(meAssetIdent, meAsset&) = 0;
	meAssetLoadStage meAssetWaitForLoad(meAssetIdent);
};

struct meAssetSystem
{
	// relative to working dir
	String resourceDir = {};
	RWLock assetRegistryLock = {};
    meMap<meAssetIdent, meAsset> assetRegistry = {};
    // meAssetType -> loader
    meAssetLoader* assetLoaders[NUM_ASSET_TYPES] = {};
	meJobSystem assetCompilerJobs = {}; // TODO: replace this with a unified job system which should have multiple "queue" types
    meEvent assetBeginLoadingEvent = {};
    meEvent assetFinishedLoadingEvent = {};
};

void meAssetInitialize(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);
void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type);

typedef void(*meAssetOnAssetLoadCb)(const meAsset&);

meAssetLoadStage* meAssetRequestLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1,
    meAssetOnAssetLoadCb cb = nullptr);

meAssetLoadStage* meAssetWaitForLoad(
	meAllocator* allocator,
	meAssetIdent* assetIdents, 
	u32 numAssets = 1);

meAsset* meAssetTryGet(meAssetIdent asset);

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
