#include "me_command.h"
#include "external/concurrentqueue/concurrentqueue.h"

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

struct meCommandQueues
{
    moodycamel::ConcurrentQueue<meExternalCommand> externalCommandQueue;
};

static void HandleCreateAsset(const meCmdCreateNewAsset& cmd)
{
	meAsset newAsset = meAssetCreateNewAsset(cmd.type, meResourceType_TemplateAsset, cmd.path);
	meJobId writeReq = meAssetRequestWriteTemplate(meSpanTyped<MAID>(&newAsset.id, 1));
	UNUSED(writeReq);
}

static void HandleChangeScene(const meCmdChangeScene& cmd)
{
	EngineContext* engine = GetEngineCtx();
	StringView sceneFile = cmd.path;
	meFsNormalizePathSeperators(sceneFile);
	engine->sceneSystem->ChangeCurrentSceneAsync(sceneFile);
}

static void HandleSaveAsset(const meCmdSaveAsset& cmd)
{
    if (!cmd.asset.IsTemplateAsset())
    {
        LOG_WARN("Support for saving non-template assets isn't fully done");
    }
    MAID maid = cmd.asset;
	meAsset* asset = meAssetTryGetTemplate(maid);
	if (!asset)
	{
        LOG_WARN("Tried to save asset, but the header doesn't point to a valid loaded asset");
		return;
	}

    StringView filePath = meAssetIndexGetFilesystemPath(maid);
	if (filePath)
	{
		meFsNormalizePathSeperators(filePath);
		filePath = meAssetEnsurePathHasGoodExtension(filePath, maid.GetType());
		filePath = meAssetGetRelPathForResource(filePath);
		meAssetIndexRegisterRelation(filePath, maid);
	}

    meAssetRequestWriteTemplate(meSpanTyped<MAID>(&maid, 1));
}

static InspectorWindow* FindInspectorForAsset(EditorContext& editor, MAID assetID)
{
    for (u32 i = 0; i < editor.inspectors.size(); i++)
    {
        if (editor.inspectors[i].currentAsset.id == assetID)
            return &editor.inspectors[i];
    }

    return nullptr;
}

static void HandlePickEntity(const meCmdPickEntity& cmd)
{
	EngineContext* engine = GetEngineCtx();
	EditorContext& editor = *engine->editor;
	meScene* currentScene = &engine->sceneSystem->CurrentScene();

	meCamera& cam = editor.editorCamera;
	glm::mat4 view = cam.GetViewFromWorldMatrix();
	glm::mat4 proj = cam.GetProjectionFromViewMatrix();

	meRay ray = meScreenPointToRay(cmd.screenPos, engine->osData->windowWidth, engine->osData->windowHeight, view, proj);

	meSceneRaycastHit hit = meSceneRaycast(*currentScene, ray);

    if (editor.editorSelectedObj != hit.entity)
    {
        if (editor.editorSelectedObj.isLoaded())
        {
            ScopedAssetLockW<meEntity> oldPickedEntity(editor.editorSelectedObj);
            if (oldPickedEntity)
            {
                SET_BIT(oldPickedEntity->flags, EntityFlags_Selected, false);
            }
        }
        if (hit)
        {
            ScopedAssetLockW<meEntity> newlyPickedEntity(hit.entity);
            if (newlyPickedEntity)
            {
                SET_BIT(newlyPickedEntity->flags, EntityFlags_Selected, true);
            }
        }
	}
	editor.editorSelectedObj = hit.entity;

    if (!hit) return;

    if (InspectorWindow* inspector = FindInspectorForAsset(editor, hit.entity.id))
    {
        inspector->active = true;
        inspector->currentAsset = hit.entity;
        return;
    }

    InspectorWindow inspector = {};
    inspector.active = true;
    inspector.currentAsset = hit.entity;
    editor.inspectors.push_back(inspector);
}

static void DispatchExternalCommand(const meExternalCommand& cmd)
{
	switch (cmd.type)
	{
		case meExternalCommandType_ChangeScene:
			HandleChangeScene(cmd.changeScene);
			break;
		case meExternalCommandType_SaveAsset:
			HandleSaveAsset(cmd.saveAsset);
			break;
		case meExternalCommandType_CreateAsset:
			HandleCreateAsset(cmd.createAsset);
			break;
		case meExternalCommandType_PickEntity:
			HandlePickEntity(cmd.pickEntity);
			break;
		case meExternalCommandType_MainThreadCmd:
			if (cmd.mainThreadCmd.fn)
			{
				cmd.mainThreadCmd.fn();
			}
			break;
	}
}

void meInitMainThreadCommandQueue()
{
    EngineContext* engine = GetEngineCtx();
    engine->commandQueues = MENEW(&engine->engineArena, meCommandQueues);
}

void meShutdownMainThreadCommandQueue()
{
    EngineContext* engine = GetEngineCtx();
    MEDELETE(&engine->engineArena, meCommandQueues, engine->commandQueues);
    engine->commandQueues = nullptr;
}

void meSendExternalCommand(meExternalCommand cmd)
{
    GetEngineCtx()->commandQueues->externalCommandQueue.enqueue(cmd);
}

void meFlushMainThreadCommands()
{
    meCommandQueues& queues = *GetEngineCtx()->commandQueues;
    meExternalCommand cmd;
    while (queues.externalCommandQueue.try_dequeue(cmd))
        DispatchExternalCommand(cmd);
}
