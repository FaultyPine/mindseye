#pragma once

#include "core/me_defines.h"
#include "core/me_string.h"
#include "core/me_math.h"
#include "asset/me_asset.h"

typedef void(*meMainThreadCommandFn)(void* userdata);

enum meExternalCommandType
{
	meExternalCommandType_ChangeScene,
	meExternalCommandType_SaveAsset,
	meExternalCommandType_CreateAsset,
	meExternalCommandType_PickEntity,
	meExternalCommandType_MainThreadCmd,
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
	meAsset asset;
};

struct meCmdPickEntity
{
	glm::vec2 screenPos;
};

struct meCmdMainThread
{
	meMainThreadCommandFn fn;
	void* userdata;
};

struct meExternalCommand
{
	meExternalCommandType type;
    // this is theoretically a tagged union, but it's really annoying writing dtor and copy ctor and all that. This is a short-lived concept, so burning the memory is fine
    meCmdChangeScene changeScene;
    meCmdSaveAsset saveAsset;
    meCmdCreateNewAsset createAsset;
    meCmdPickEntity pickEntity;
	meCmdMainThread mainThreadCmd;
};

// Enqueue an external command to be dispatched on the main thread during the next flush.
MEAPI void meSendExternalCommand(meExternalCommand cmd);

// Thread-safe queue of work to run on the main thread. Enqueue from any thread,
// flush once per frame from the main thread.
MEAPI void meInitMainThreadCommandQueue();
MEAPI void meShutdownMainThreadCommandQueue();
MEAPI void meFlushMainThreadCommands();
