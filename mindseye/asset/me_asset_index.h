#pragma once

#include "asset/me_asset.h"
#include "core/containers/me_map.h"
// Main purpose: map MAID to asset path on disk

struct meAssetIndex
{
	meMap<MAID, StringView> filesystemPaths = {};
};

StringView meAssetIndexGetFilesystemPath(
	const MAID& maid);


void meAssetIndexInitialize(EngineContext* engine);
