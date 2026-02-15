#pragma once

#include "asset/me_asset.h"
#include "core/containers/me_map.h"
// Main purpose: map MAID to asset path on disk

struct meAssetIndex
{
	meMap<MAID, String> assetToPathMap = {};
	meMap<String, MAID> pathToAssetsMap = {};
	meMap < MAID, u32 > serializedUniqueIdentifiers = {};
};

StringView meAssetIndexGetFilesystemPath(
	const MAID& maid);

MAID meAssetIndexGetMAIDFromPath(
	const StringView& path);

void meAssetIndexRegisterRelation(
	const StringView& path,
	const MAID& maid);

void meAssetIndexInitialize(EngineContext* engine);
