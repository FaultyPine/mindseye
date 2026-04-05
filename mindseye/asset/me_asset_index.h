#pragma once

#include "asset/me_asset.h"
#include "core/containers/me_map.h"
// Main purpose: map MAID to asset path on disk

struct meAssetIndex
{
	meMap<MAID, String> assetToPathMap = {};
	meMap<String, MAID> pathToAssetsMap = {};
	meMap<MAID, u32> serializedUniqueIdentifiers = {};
};

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
