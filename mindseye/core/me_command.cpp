#include "me_command.h"

#include "core/me_app.h"
#include "core/me_log.h"
#include "asset/me_asset.h"
#include "scene/me_scene.h"
#include "scene/me_entity.h"
#include "platform/me_os.h"
#include "core/me_filesystem.h"

static void HandleCreateNewScene(const meCmdCreateNewScene& cmd)
{
	meAsset newAsset = meAssetCreateNew(MAScene, cmd.path);
	meJobId writeReq = meAssetRequestWrite(meSpanTyped<meAssetIdent>(&newAsset.ident, 1));
	UNUSED(writeReq);
}

static void HandleChangeScene(const meCmdChangeScene& cmd)
{
	EngineContext* engine = GetEngineCtx();
	StringView sceneFile = cmd.path;
	meFsNormalizePathSeperators(sceneFile);
	engine->sceneSystem->ChangeCurrentScene(sceneFile);
}

static void HandleSaveCurrentScene(const meCmdSaveCurrentScene& cmd)
{
	EngineContext* engine = GetEngineCtx();
	meScene* currentScene = &engine->sceneSystem->CurrentScene();
	meAsset* asset = meAssetTryGet(currentScene->header);
	if (!asset)
	{
		return;
	}

	if (!asset->ident.diskIdent && cmd.path)
	{
		StringView sceneFile = cmd.path;
		meFsNormalizePathSeperators(sceneFile);
		sceneFile = meAssetEnsurePathHasGoodExtension(sceneFile, MAScene);
		sceneFile = meAssetGetRelPathForResource(sceneFile);
		asset->ident.diskIdent = sceneFile;
	}

	if (asset->ident.diskIdent)
	{
		meAssetRequestWrite(meSpanTyped<meAssetIdent>(&currentScene->header, 1));
	}
}

static void HandleCreateEntity(const meCmdCreateEntity& cmd)
{
	UNUSED(cmd);
	EngineContext* engine = GetEngineCtx();
	meScene* currentScene = &engine->sceneSystem->CurrentScene();
	EntityRef newEnt = Entity::CreateBlankEntity();
	DynArrayPush(currentScene->entities, newEnt);
}

void meReceiveExternalCommand(meExternalCommand cmd)
{
	switch (cmd.type)
	{
		case meExternalCommandType_CreateNewScene:
			HandleCreateNewScene(cmd.createNewScene);
			break;
		case meExternalCommandType_ChangeScene:
			HandleChangeScene(cmd.changeScene);
			break;
		case meExternalCommandType_SaveCurrentScene:
			HandleSaveCurrentScene(cmd.saveCurrentScene);
			break;
		case meExternalCommandType_CreateEntity:
			HandleCreateEntity(cmd.createEntity);
			break;
	}
}
