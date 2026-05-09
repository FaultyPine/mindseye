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
        LOG_WARN("Tried to save current scene, but scene header doesn't point to a valid loaded asset");
		return;
	}

    StringView sceneFile = meAssetIndexGetFilesystemPath(maid);
	if (sceneFile)
	{
		meFsNormalizePathSeperators(sceneFile);
		sceneFile = meAssetEnsurePathHasGoodExtension(sceneFile, maid.GetType());
		sceneFile = meAssetGetRelPathForResource(sceneFile);
		meAssetIndexRegisterRelation(sceneFile, maid);
	}

    meAssetRequestWriteTemplate(meSpanTyped<MAID>(&maid, 1));
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

    if (editor.editorSelectedObj != hit.entity)
    {
        if (editor.editorSelectedObj.isLoaded())
        {
            meEntity& oldPickedEntity = meEntityGet(editor.editorSelectedObj);
            SET_BIT(oldPickedEntity.flags, EntityFlags_Selected, false);
        }
        if (hit)
        {
            meEntity& newlyPickedEntity = meEntityGet(hit.entity);
            SET_BIT(newlyPickedEntity.flags, EntityFlags_Selected, true);
        }
    }
	editor.editorSelectedObj = hit.entity;
    InspectorWindow inspector = {};
    inspector.active = true;
    inspector.currentAsset = hit.entity;
    editor.inspectors.push_back(inspector);
}

struct meCommandEntry
{
    meMainThreadCommandFn fn;
    void* data;
};

static moodycamel::ConcurrentQueue<meCommandEntry> g_commandQueue;

void meInitMainThreadCommandQueue() {}

void meEnqueueMainThreadCommand(meMainThreadCommandFn fn, void* data)
{
    g_commandQueue.enqueue({ fn, data });
}

void meFlushMainThreadCommands()
{
    meCommandEntry entry;
    while (g_commandQueue.try_dequeue(entry))
        entry.fn(entry.data);
}

void meReceiveExternalCommand(meExternalCommand cmd)
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
	}
}
