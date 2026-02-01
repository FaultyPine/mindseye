
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/core/me_math.h"
#include "mindseye/render/me_mesh.h"
#include "mindseye/scene/me_transform.h"

void testbed_onsceneload(EngineContext* engine)
{
    //GameGlobals& globals = *MENEW(&engine->gameArena, GameGlobals);
	EntityRef testEntity = Entity::CreateEntity(STRING_LIT("bruh"), meTransform(glm::vec3(sin(GetTimeUsec()) * 5.0, cos(GetTimeUsec()) * 5.0, 0.0)));
	DynArrayPush(engine->sceneSystem->CurrentScene().entities, testEntity);
	EntityData& entity = Entity::GetEntity(testEntity);
	//Entity::SetFlag(entity, EntityFlags_NoSer, true);
	entity.mesh = meAsset(GenPlaneMesh(2));
}

void testbed_init(EngineContext* engine)
{
    engine->appCallbacks.onSceneLoadFn = testbed_onsceneload;
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