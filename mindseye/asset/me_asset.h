#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "core/thread/me_rw_lock.h"
#include "core/me_job_system.h"
#include "core/me_event.h"
#include "core/containers/me_array.h"
#include "core/me_resourcepool.h"

#include "generatedtypes/me_asset.generated.h"

#define ME_DECLARE_ASSET_TYPES \
X(MABadData, "")\
X(MAScene, "scn")\
X(MAShader, "shd")\
X(MAMaterial, "mat")\
X(MAMesh, "mesh")\
X(MATexture, "tex")\
X(MAEntity, "ent")

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
struct MEREFLECT(type) 
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
    template <meAssetType T>
	static MAID Of()
	{
		MAID m;
		m.SetType(T);
		m.SetID(U32_INVALID_ID);
		return m;
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
};

MEMAP_BEGIN_CUSTOM_HASHER(MAID, obj) 
{
    size_t h1 = std::hash<u64>{}(obj.GetType());
	size_t h2 = std::hash<u64>{}(obj.GetID());
	return HashCombine(h1, h2);
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

#define ME_ASSET_HEADER_FIELDNAME "header"

#define ME_ASSET_STRUCTURE(typeName) \
MAID header = {}; \
typeName(const MAID& ident) : typeName() { header = ident; } \
typeName() = default;


// storing both the "load-time" and "usage-time" information, this is meant to be
// both serialized, and also used for loading at runtime. This is what will be in the fields
// of asset definitions. I.E. when a "scene" asset references a "mesh" asset, use this structure
struct MEREFLECT(type, Serializer="meAssetSerializerToStringFn", Deserializer="meAssetDeserializerFromStringFn", Equals="meAssetEqualsFn") 
meAsset
{
    // refers to a template asset on disk
	MAID id = {};
    // NOTE: a given meAsset can refer to a "template asset" which is an asset on disk
    // OR an "instance" asset, which is generally a copy of a template asset, used at runtime
	MEREFLECT(exclude)
	Eye runtimeHandle = EYE_INVALID;
	MEREFLECT(exclude)
    meAssetLoadStage loadStage = Unloaded;
	
    bool IsTemplateAsset() const { return runtimeHandle.IsTemplateAsset(); }

	meAsset(const MAID identifier) : 
		id(identifier)
	{}
	meAsset(const MAID identifier, meAssetLoadStage stage) :
		id(identifier), loadStage(stage)
	{}
	// initialized with both "load-time" and "usage-time" info
	meAsset(Eye eye, const MAID& identifier) : 
		id(identifier), runtimeHandle(eye) 
	{
		if (runtimeHandle)
		{
			loadStage = Loaded;
		}
	}
	// can be initialized as a usage-only concept. 
	// I.E. Generating meshes/etc on-the-fly without an associated on-disk asset.
	meAsset(Eye eye, meAssetType type) : runtimeHandle(eye)
	{
		if (runtimeHandle)
		{
			loadStage = Loaded;
		}
        id.SetType(type);
	}
	meAsset() = default;

	bool isLoaded() const 
	{
		return runtimeHandle != EYE_INVALID && loadStage == Loaded; 
	}
	operator const Eye() const { return runtimeHandle; }
	operator const MAID() const { return id; }
};

template <meAssetType AssetTypeV>
struct meTypedAsset : public meAsset
{
    static constexpr meAssetType AssetType = AssetTypeV;
    meTypedAsset() : meAsset(MAID::Of<AssetTypeV>()) {}
    meTypedAsset(const meAsset& other) : meAsset(other) 
    {
        ME_ASSERT(other.id.GetType() == AssetTypeV);
    }
    using meAsset::meAsset;
};

struct meAssetLoader
{
    // called on asset threads
	virtual void meAssetLoad(meAsset&);
	// TODO: return a serialized buffer, rather than writing to disk inside this func
	virtual void meAssetWrite(meAsset&);
	virtual void meAssetOnLoad(meAsset&) {}
	meAssetLoadStage meAssetWaitForLoadstage(
		meSpanTyped<MAID> assets, 
		meAssetLoadStage);
	meAssetLoadStage meAssetWaitForLoadstage(
		const MAID&, 
		meAssetLoadStage);

	meAssetLoader(meTypeDescriptor* typedesc, meResourcePoolBase* pool, meAssetType type) 
	: assetTypeDesc(typedesc), resourcePool(pool), assetType(type)
	{}

	meTypeDescriptor* assetTypeDesc = nullptr;
	meResourcePoolBase* resourcePool = nullptr;
	meAssetType assetType = MABadData;
};

struct meAssetTypeRegistry
{
	RWLock lock = {};
	meMap<MAID, meAsset> templateAssets = {};
};

struct meAssetSystem
{
	// relative to working dir
	String resourceDir = {};
	// per-asset-type registries, each with their own RWLock
	meArray<meAssetTypeRegistry, NUM_ASSET_TYPES> registries = {};
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
void meAssetInitializeLate(EngineContext* engine);
void meAssetTeardown(EngineContext* engine);
void meAssetRegisterLoader(meAssetLoader* loader);

// creates a default-constructed instance of an asset type on disk (and assigns it a proper guid and all that)
MEAPI meAsset meAssetCreateNewTemplateAsset(
	meAssetType type,
	StringView filename = {});

MEAPI meAsset meAssetCreateNewInstanceAsset(meAssetType type);

typedef void(*meAssetOnAssetLoadCb)(const meAsset&);

MEAPI meJobId meAssetRequestLoadTemplate(
	MAID* assetIdents, 
	u32 numAssets = 1,
    meAssetOnAssetLoadCb cb = nullptr);

// assets passed in do not have to be the actual meAsset's in the asset registry
// this reaches into the registry to do necessary bookeeping
// Also if the resource type has a "Destroy()" member fn, it'll call that before the dtor is called
MEAPI void meAssetUnloadBlocking(meSpanTyped<meAsset> assets);

MEAPI bool meAssetWaitUntilLoadstage(
	meSpanTyped<MAID> assetIdents,
	meAssetLoadStage loadStage);

MEAPI meJobId meAssetRequestWrite(
	meSpanTyped<MAID> assetIdents,
	meAssetOnAssetLoadCb onWriteCb = nullptr);

MEAPI meAsset* meAssetTryGetTemplate(MAID assetID);

MEAPI void meAssetSetResourceDir(StringView dir);

MEAPI StringView meAssetGetResourceDir();

// I.E. "models/obj.gltf" -> "C:/workingdir/mindseye/bin/resource/models/obj.gltf" or something similar
// returns a short-lived string. This should only be used for "scratch" operations. If you need to store this string long-term,
// copy it, or use something else
// NOTE: allocates a temporary buffer
MEAPI StringView meAssetGetAbsPathForResource(StringView resourcePath);
// does the opposite of the above func
// takes an abs path on disk and converts it to be relative to the "data directory"
// NOTE: allocates and returns a temporary buffer
MEAPI StringView meAssetGetRelPathForResource(StringView resourcePath);


// takes a buffer that has been deserialized from an asset (I.E. SerializeFromFile)
// and attempts to find a member field that matches what is declared by
// ME_ASSET_STRUCTURE. Use this to get the asset ident out of any asset type's deserialized buffer
// returns a buffer pointing to the asset ident field data if present, otherwise an invalid mespan 
meSpan meSerializeTryGetAssetHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer);

StringView meAssetFileExtFromType(meAssetType type);

StringView meAssetEnsurePathHasGoodExtension(
    const StringView& assetPath, 
    meAssetType inputType);
