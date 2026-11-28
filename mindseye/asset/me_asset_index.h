#pragma once

#include "asset/me_asset.h"
#include "core/containers/me_map.h"
#include "core/containers/dynarray.h"
#include "core/containers/me_span.h"
// Main purpose: map MAID to asset path on disk

struct meAssetIndex
{
	meMap<MAID, String> assetToPathMap = {};
	meMap<String, MAID> pathToAssetsMap = {};
	meMap<MAID, u32> serializedUniqueIdentifiers = {};
	meMap<MAID, DynArray<MAID>> dependencyMap = {};
    meAtomicU64 nextGeneratedAssetID{};
    bool initialized = false;
};

// Calling multiple times with the same pair currently has no special de-duping behavior
MEAPI void meAssetIndexRecordDependency(MAID owner, MAID dep);

MEAPI meSpanTyped<MAID> meAssetIndexGetDependencies(MAID owner);

MEAPI StringView meAssetIndexGetFilesystemPath(
	const MAID& maid);

MEAPI MAID meAssetIndexGetMAIDFromPath(
	const StringView& path);

MEAPI u32 meAssetIndexGetUniqueID(MAID maid);

// atomically reserves a unique asset id.
// this id is not persisted until an asset that uses the id is persisted
MEAPI u64 meAssetIndexReserveAssetID(meAssetType type);

void meAssetIndexRegisterRelation(
	const StringView& path,
	const MAID& maid);

meAssetType meAssetFindAssetTypeFromFilepath(
    StringView path);

void meAssetIndexInitialize(EngineContext* engine);

const meAssetIndex& meAssetIndexGetRO();
meAssetIndex& meAssetIndexGet();
