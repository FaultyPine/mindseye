#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "core/thread/me_rw_lock.h"
#include "core/me_job_system.h"
#include "core/me_event.h"
#include "core/containers/me_array.h"

#include "generatedtypes/me_asset.generated.h"

#define ME_DECLARE_ASSET_TYPES \
X(MABadData, "")\
X(MAScene, "scn")\
X(MAShader, "shd")\
X(MAMaterial, "mat")\
X(MAMesh, "mesh")\
X(MATexture, "tex")

#define ME_ASSET_EXTENSION ".masset"

enum meAssetType
{
#define X(name, ext) name,
ME_DECLARE_ASSET_TYPES
#undef X
    NUM_ASSET_TYPES
};
STATIC_ASSERT(NUM_ASSET_TYPES < 255);

StringView meAssetTypeToString(meAssetType type);

constexpr static u32 MAID_ID_BITS = 48; // lower bits
constexpr static u32 MAID_TYPE_BITS = 8; // top bits

// Mindseye Asset ID
struct MEREFLECT(type, Serializer=MAIDSerializerToStringFn) 
MAID
{
	// TODO: for shipping builds, we can pack things, or give more bits to the id
	// for debugging, it's nice to have two separate things
	u32 id = U32_INVALID_ID;
	u32 type = U32_INVALID_ID;
	
    MAID() = default;
    MAID(u64 id, meAssetType type);
	template <meAssetType T>
	MAID(u64 id)
	{
		SetType(T);
		SetID(id);
	}
    operator bool() const 
	{
		meAssetType type = (meAssetType)GetType();
		return GetID() != U32_INVALID_ID && type < NUM_ASSET_TYPES && type > MABadData; 
	}
    bool operator==(const MAID& other) const { return GetType() == other.GetType() && GetID() == other.GetID(); }
	inline meAssetType GetType() const
	{
		return (meAssetType)type;
	}
    inline void SetType(meAssetType type)
    {
		this->type = type;
    }
	inline u64 GetID() const
	{
		return id;
	}
    inline void SetID(u64 id)
    {
		ME_ASSERT(id <= ME_UINT_MAX);
		this->id = id;
    }


	//inline u64 GetType() const
	//{
	//	return idAndType >> MAID_ID_BITS;
	//}
 //   inline void SetType(meAssetType type)
 //   {
 //       u64 typefull = (u64)type;
 //       typefull = typefull << MAID_ID_BITS;
	//	idAndType &= (~0ull >> MAID_TYPE_BITS); // clear type bits
 //       idAndType |= typefull;
 //   }
	//inline u64 GetID() const
	//{
	//	return idAndType & (~0ull >> MAID_TYPE_BITS);
	//}
 //   inline void SetID(u64 id)
 //   {
 //       // make sure top type bits aren't set
 //       ME_ASSERT(id == (id & ~(((u64)0xff) << MAID_ID_BITS)));
	//	idAndType &= (~0ull << MAID_ID_BITS); // clear all id bits
 //       idAndType |= id; // set id bits
 //   }
};
MEMAP_BEGIN_CUSTOM_HASHER(MAID, obj) 
{
    size_t h1 = std::hash<u64>{}(obj.GetType());
	size_t h2 = std::hash<u64>{}(obj.GetID());
	return HashCombine(h1, h2);
} MEMAP_END_CUSTOM_HASHER

StringView MAIDSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx);

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

#define ME_ASSET_STRUCTURE(typeName) \
meAssetIdent header = {}; \
typeName(const meAssetIdent& ident) : typeName() { header = ident; } \
typeName() = default;

// identifies an asset "on disk".
// these can map to filesystem paths, or something else if assets are being loaded/fetched from some other mechanism
struct MEREFLECT(type) meAssetIdent
{
    MAID id = MAID_INVALID;
	MEREFLECT(exclude) String diskIdent = {};
	// treated opaquely as a identifier for a single verion of a single asset
	// could be a hash, timestamp, or something else
	MEREFLECT(exclude) u32 assetUniqueIdentifier = 0;
    meAssetIdent() = default;
    bool operator==(const meAssetIdent& other) const 
	{ 
		return id == other.id && assetUniqueIdentifier == other.assetUniqueIdentifier &&
			((other.diskIdent.len == 0 || diskIdent.len == 0) || other.diskIdent == diskIdent);
	}
	operator bool() const 
	{
		return id;
	}
};

MEMAP_BEGIN_CUSTOM_HASHER(meAssetIdent, ident) 
{
    size_t h1 = std::hash<MAID>{}(ident.id);
    size_t h2 = std::hash<u32>{}(ident.assetUniqueIdentifier);
	return HashCombine(h1, h2);
} MEMAP_END_CUSTOM_HASHER

// storing both the "load-time" and "usage-time" information, this is meant to be
// both serialized, and also used for loading at runtime. This is what will be in the fields
// of asset definitions. I.E. when a "scene" asset references a "mesh" asset, use this structure
struct MEREFLECT(type) meAsset
{
	meAssetIdent ident = {};
	MEREFLECT(exclude)
	Eye runtimeHandle = {};
	MEREFLECT(exclude)
    meAssetLoadStage loadStage = Unloaded;
	
	meAsset(const meAssetIdent& identifier) : 
		ident(identifier)
	{}
	meAsset(const meAssetIdent& identifier, meAssetLoadStage stage) :
		ident(identifier), loadStage(stage)
	{}
	// initialized with both "load-time" and "usage-time" info
	meAsset(Eye eye, const meAssetIdent& identifier) : 
		ident(identifier), runtimeHandle(eye) 
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
	operator const MAID() const { return ident.id; }
};

struct meAssetLoader
{
	// TODO: these could have defaults that just 
	// serializes/deserializes opaquely. Systems can then add their own custom stuff
	// and call into this base functionality
	// additionally in order to do the above, there should be a "generic" way to access
	// each asset's resourcepool by just the meAssetType
	// instead of bespoke pointers in the EngineContext, there should be a list NUM_ASSET_TYPES large of meResourcePool* base classes
	// TODO: put the resourcepool in this struct!

    // called on asset threads
    virtual void meAssetLoad(meAsset&) = 0;
	virtual void meAssetWrite(meAsset&) = 0;
	virtual void meAssetCreate(StringView) = 0;
	virtual const meTypeDescriptor& meAssetGetTypeDescriptor() = 0;
	virtual void meAssetOnLoad(meAsset&) {}
	meAssetLoadStage meAssetWaitForLoadstage(
		const meAssetIdent&, 
		meAssetLoadStage);
};

struct meAssetSystem
{
	// relative to working dir
	String resourceDir = {};
	RWLock assetRegistryLock = {};
    meMap<meAssetIdent, meAsset> assetRegistry = {};
    // meAssetType -> loader
    meArray<meAssetLoader*, NUM_ASSET_TYPES> assetLoaders = {};
	meJobSystem assetCompilerJobs = {}; // TODO: replace this with a unified job system which should have multiple "queue" types
    meEvent assetBeginLoadingEvent = {};
    meEvent assetFinishedLoadingEvent = {};
	meEvent assetBeganWritingEvent = {};
	meEvent assetFinishedWritingEvent = {};
};

meAssetSystem& meAssetSystemGet();
void meAssetInitialize(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);
void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type);

// creates a default-constructed instance of an asset type on disk (and assigns it a proper guid and all that)
void meAssetCreateNew(
	StringView filename,
	meAssetType type);

meAssetIdent meAssetGetIdentFromPath(
	StringView path);

typedef void(*meAssetOnAssetLoadCb)(const meAsset&);

meJobId meAssetRequestLoad(
	meAssetIdent* assetIdents, 
	u32 numAssets = 1,
    meAssetOnAssetLoadCb cb = nullptr);

bool meAssetWaitUntilLoadstage(
	meSpanTyped<meAssetIdent> assetIdents,
	meAssetLoadStage loadStage);

meJobId meAssetRequestWrite(
	meSpanTyped<meAssetIdent> assetIdents,
	meAssetOnAssetLoadCb onWriteCb = nullptr);

meAsset* meAssetTryGet(meAssetIdent asset);

void meAssetSetResourceDir(StringView dir);

StringView meAssetGetResourceDir();

// I.E. "models/obj.gltf" -> "C:/workingdir/mindseye/bin/resource/models/obj.gltf" or something similar
// returns a short-lived string. This should only be used for "scratch" operations. If you need to store this string long-term,
// copy it, or use something else
// NOTE: allocates a temporary buffer
StringView meAssetGetAbsPathForResource(StringView resourcePath);
// does the opposite of the above func
// takes an abs path on disk and converts it to be relative to the "data directory"
// NOTE: allocates and returns a temporary buffer
StringView meAssetGetRelPathForResource(StringView resourcePath);


// takes a buffer that has been deserialized from an asset (I.E. SerializeFromFile)
// and attempts to find a member field that matches what is declared by
// ME_ASSET_STRUCTURE. Use this to get the asset ident out of any asset type's deserialized buffer
// returns a buffer pointing to the asset ident field data if present, otherwise an invalid mespan 
meSpan meSerializeTryGetAssetIdentHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer);
