#pragma once

#include "asset/me_asset.h"
#include "core/containers/me_map.h"
// Main purpose: map MAID to asset path on disk

struct meAssetIndex
{
	meMap<MAID, String> assetToPathMap = {};
	meMap<String, MAID> pathToAssetsMap = {};
};

StringView meAssetIndexGetFilesystemPath(
	const MAID& maid);

MAID meAssetIndexGetMAIDFromPath(
	const StringView& path);

void meAssetIndexInitialize(EngineContext* engine);
