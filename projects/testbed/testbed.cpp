
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/core/me_math.h"
#include "mindseye/render/me_mesh.h"
#include "mindseye/scene/me_transform.h"

void testbed_onsceneload(meEventPayload payload)
{

}

void testbed_init(EngineContext* engine)
{
    meEventSubscribe(engine->userApp.ActiveCallbacks().onSceneLoaded, testbed_onsceneload);
    //GameGlobals& globals = *MENEW(&engine->gameArena, GameGlobals);
	meTypedAsset<MAEntity> testEntity = meEntityCreateBlankInstance(STRING_LIT("bruh"));
	DynArrayPush(engine->sceneSystem->CurrentScene().entities, testEntity);
	meEntity& entity = meEntityGet(testEntity);
	entity.transform = meTransform(glm::vec3(sin(GetTimeUsec()) * 5.0, cos(GetTimeUsec()) * 5.0, 0.0));
	//meEntitySetFlag(entity, EntityFlags_NoSer, true);
    meAsset newMesh = meAssetCreateNewAsset(MAMesh, meResourceType_InstanceAsset);
    GenPlaneMesh(newMesh, 2);
	entity.mesh = newMesh;
	entity.authoritativeBounds = meMeshPoolGet().Get(newMesh).meshBounds;
}

void testbed_update(EngineContext* engine)
{
	//GameGlobals& globals = *((GameGlobals*)engine->gameArena.backing_mem);
	//ME_ASSERT(globals.IsValid());
	//meEntity& entity = meEntityGet(globals.testEntity);
	//entity.transform.position.x += sin(GetTimeSec());
    auto entities = engine->sceneSystem->CurrentScene().entities;
    for (DynArray_Foreach(entities, i))
    {
        auto& entityAsset = entities[i];
        auto& entity = engine->entityPool->Get(entityAsset.runtimeHandle);
        entity.transform.position.x = sinf(GetTimeSec()) * 5.0f;
    }
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);