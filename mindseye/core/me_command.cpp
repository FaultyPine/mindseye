#include "me_command.h"

#include "core/me_app.h"
#include "core/me_log.h"
#include "core/me_math.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"
#include "scene/me_scene.h"
#include "scene/me_entity.h"
#include "platform/me_os.h"
#include "core/me_filesystem.h"
#include "editor/me_editor.h"

#include "external/potable-file-dialogs.h"

static void HandleCreateNewScene(const meCmdCreateNewScene& cmd)
{
	meAsset newAsset = meAssetCreateNew(MAScene, cmd.path);
	meJobId writeReq = meAssetRequestWrite(meSpanTyped<MAID>(&newAsset.id, 1));
	UNUSED(writeReq);
}

static void HandleChangeScene(const meCmdChangeScene& cmd)
{
	EngineContext* engine = GetEngineCtx();
	StringView sceneFile = cmd.path;
	meFsNormalizePathSeperators(sceneFile);
	engine->sceneSystem->ChangeCurrentScene(sceneFile);
}

static void HandleSaveCurrentScene(const meCmdSaveCurrentScene& cmdIn)
{
    meCmdSaveCurrentScene cmd = cmdIn;
	EngineContext* engine = GetEngineCtx();
	meScene* currentScene = &engine->sceneSystem->CurrentScene();

    // if the scene has no disk path yet, ask the user for one
    // kept in outer scope so the string data outlives the command dispatch
    std::vector<std::string> openFileResult;
    StringView fsPath = meAssetIndexGetFilesystemPath(currentScene->header);
    if (!fsPath && !cmd.path)
    {
        openFileResult = pfd::open_file("Location to save the scene file", ".").result();
        if (!openFileResult.empty())
        {
            ME_ASSERT(openFileResult.size() == 1);
            const char* fileCstr = openFileResult[0].c_str();
            cmd.path = StringFromCString(fileCstr);
        }
    }

	meAsset* asset = meAssetTryGet(currentScene->header);
	if (!asset)
	{
		return;
	}

	if (!fsPath && cmd.path)
	{
		StringView sceneFile = cmd.path;
		meFsNormalizePathSeperators(sceneFile);
		sceneFile = meAssetEnsurePathHasGoodExtension(sceneFile, MAScene);
		sceneFile = meAssetGetRelPathForResource(sceneFile);
		meAssetIndexRegisterRelation(sceneFile, asset->id);
	}

    meAssetRequestWrite(meSpanTyped<MAID>(&currentScene->header, 1));
}

static void HandleCreateEntity(const meCmdCreateEntity& cmd)
{
	UNUSED(cmd);
	EngineContext* engine = GetEngineCtx();
	meScene* currentScene = &engine->sceneSystem->CurrentScene();
	meAsset newEnt = meEntityCreateBlank();
	DynArrayPush(currentScene->entities, newEnt);
}

static void HandlePickEntity(const meCmdPickEntity& cmd)
{
	EngineContext* engine = GetEngineCtx();
	EditorContext& editor = *engine->editor;
	meScene* currentScene = &engine->sceneSystem->CurrentScene();

	meCamera& cam = editor.editorCamera;
	glm::mat4 view = cam.GetViewMatrix();
	glm::mat4 proj = cam.GetProjectionMatrix();

	meRay ray = meScreenPointToRay(cmd.screenPos, engine->osData->windowWidth, engine->osData->windowHeight, view, proj);

	meSceneRaycastHit hit = meSceneRaycast(*currentScene, ray);

    if (editor.selectedEntity != hit.entity)
    {
        if (editor.selectedEntity)
        {
            meEntity& oldPickedEntity = meEntityGet(editor.selectedEntity);
            SET_BIT(oldPickedEntity.flags, EntityFlags_Selected, false);
        }
        if (hit)
        {
            meEntity& newlyPickedEntity = meEntityGet(hit.entity);
            SET_BIT(newlyPickedEntity.flags, EntityFlags_Selected, true);
        }
    }
	editor.selectedEntity = hit.entity;
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
		case meExternalCommandType_PickEntity:
			HandlePickEntity(cmd.pickEntity);
			break;
	}
}
