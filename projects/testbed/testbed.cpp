
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/core/me_math.h"
#include "mindseye/render/me_mesh.h"
#include "mindseye/scene/me_transform.h"

void testbed_onsceneload(meEventPayload payload)
{
    meAsset* sceneAsset = (meAsset*)payload.payload;
    UNUSED(sceneAsset);
    EngineContext* engine = GetEngineCtx();
    //GameGlobals& globals = *MENEW(&engine->gameArena, GameGlobals);
	EntityRef testEntity = Entity::CreateBlankEntity(STRING_LIT("bruh"));
	DynArrayPush(engine->sceneSystem->CurrentScene().entities, testEntity);
	EntityData& entity = Entity::GetEntity(testEntity);
	entity.transform = meTransform(glm::vec3(sin(GetTimeUsec()) * 5.0, cos(GetTimeUsec()) * 5.0, 0.0));
	//Entity::SetFlag(entity, EntityFlags_NoSer, true);
	entity.mesh = meAssetRegisterRuntime(GenPlaneMesh(2), MAMesh);
}

void testbed_init(EngineContext* engine)
{
    meEventSubscribe(engine->appCallbacks.onSceneLoaded, testbed_onsceneload);
}

void testbed_update(EngineContext* engine)
{
	//GameGlobals& globals = *((GameGlobals*)engine->gameArena.backing_mem);
	//ME_ASSERT(globals.IsValid());
	//EntityData& entity = Entity::GetEntity(globals.testEntity);
	//entity.transform.position.x += sin(GetTimeSec());
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);