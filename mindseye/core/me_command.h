#pragma once

#include "core/me_defines.h"
#include "core/me_string.h"

enum meExternalCommandType
{
	meExternalCommandType_CreateNewScene,
	meExternalCommandType_ChangeScene,
	meExternalCommandType_SaveCurrentScene,
	meExternalCommandType_CreateEntity,
};

struct meCmdCreateNewScene
{
	StringView path;
};

struct meCmdChangeScene
{
	StringView path;
};

struct meCmdSaveCurrentScene
{
	// if the current scene asset has no disk path, this will be used
	StringView path;
};

struct meCmdCreateEntity
{
};

struct meExternalCommand
{
	meExternalCommandType type;
	union
	{
		meCmdCreateNewScene createNewScene;
		meCmdChangeScene changeScene;
		meCmdSaveCurrentScene saveCurrentScene;
		meCmdCreateEntity createEntity;
	};
};

MEAPI void meReceiveExternalCommand(meExternalCommand cmd);
