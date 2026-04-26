#pragma once

#include "core/me_defines.h"
#include "core/me_string.h"
#include "core/me_math.h"
#include "asset/me_asset.h"

enum meExternalCommandType
{
	meExternalCommandType_ChangeScene,
	meExternalCommandType_SaveAsset,
	meExternalCommandType_CreateAsset,
	meExternalCommandType_PickEntity,
};

struct meCmdCreateNewAsset
{
    meAssetType type;
	String path;
};

struct meCmdChangeScene
{
	String path;
};

struct meCmdSaveAsset
{
	MAID asset;
};

struct meCmdPickEntity
{
	glm::vec2 screenPos;
};

struct meExternalCommand
{
	meExternalCommandType type;
    // this is theoretically a tagged union, but it's really annoying writing dtor and copy ctor and all that so meh
    meCmdChangeScene changeScene;
    meCmdSaveAsset saveAsset;
    meCmdCreateNewAsset createAsset;
    meCmdPickEntity pickEntity;

};

MEAPI void meReceiveExternalCommand(meExternalCommand cmd);
