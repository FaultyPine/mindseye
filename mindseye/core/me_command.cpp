#include "me_command.h"

#include "core/me_app.h"
#include "core/me_log.h"
#include "core/me_math.h"
#include "asset/me_asset.h"
#include "scene/me_scene.h"
#include "scene/me_entity.h"
#include "platform/me_os.h"
#include "core/me_filesystem.h"
#include "editor/me_editor.h"

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

static void HandlePickEntity(const meCmdPickEntity& cmd)
{
	EngineContext* engine = GetEngineCtx();
	EditorContext& editor = *engine->editor;
	meScene* currentScene = &engine->sceneSystem->CurrentScene();

	meCamera& cam = editor.editorCamera;
	glm::mat4 view = cam.GetViewMatrix();
	glm::mat4 proj = cam.GetProjectionMatrix();

	meRay ray = meScreenPointToRay(cmd.screenPos, engine->osData->windowWidth, engine->osData->windowHeight, view, proj);

	EntityRef closestEntity = {};
	f32 closestT = FLT_MAX;

	DynArray<EntityRef>& entities = currentScene->entities;
	for (DynArray_Foreach(entities, i))
	{
		EntityRef entRef = entities[i];
		EntityData& entData = Entity::GetEntity(entRef);
		if (Entity::IsFlag(entRef, EntityFlags_HIDDEN) || Entity::IsFlag(entRef, EntityFlags_DISABLED))
			continue;

		// Transform the AABB by the entity's position
		glm::vec3 worldMin = entData.transform.position + entData.authoritativeBounds.min;
		glm::vec3 worldMax = entData.transform.position + entData.authoritativeBounds.max;

		f32 t = 0.0f;
		if (meRayIntersectsAABB(ray, worldMin, worldMax, t))
		{
			if (t < closestT)
			{
				closestT = t;
				closestEntity = entRef;
			}
		}
	}

    if (editor.selectedEntity != closestEntity)
    {
        // just picked a new entity
        if (editor.selectedEntity)
        {
            EntityData& oldPickedEntity = Entity::GetEntity(editor.selectedEntity);
            SET_BIT(oldPickedEntity.flags, EntityFlags_Selected, false);
        }
        EntityData& newlyPickedEntity = Entity::GetEntity(closestEntity);
        SET_BIT(oldPickedEntity.flags, EntityFlags_Selected, true);
    }
	editor.selectedEntity = closestEntity;
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
