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
};

// Calling multiple times with the same pair currently has no special de-duping behavior
MEAPI void meAssetIndexRecordDependency(MAID owner, MAID dep);

// Returns a read-only view of the dependencies recorded for 'owner'.
// The span points into memory owned by the asset index — do not mutate or
// store it past any call that could modify dependencyMap.
MEAPI meSpanTyped<MAID> meAssetIndexGetDependencies(MAID owner);

MEAPI StringView meAssetIndexGetFilesystemPath(
	const MAID& maid);

MEAPI MAID meAssetIndexGetMAIDFromPath(
	const StringView& path);

MEAPI u32 meAssetIndexGetUniqueID(MAID maid);

void meAssetIndexRegisterRelation(
	const StringView& path,
	const MAID& maid);

meAssetType meAssetFindAssetTypeFromFilepath(
    StringView path);

void meAssetIndexInitialize(EngineContext* engine);
const meAssetIndex& meAssetIndexGetRO();
