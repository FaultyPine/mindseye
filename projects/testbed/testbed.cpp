
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/core/me_math.h"
#include "mindseye/render/me_mesh.h"
#include "mindseye/scene/me_transform.h"

struct GameGlobals
{
	bool initialized = false;
	EntityRef testEntity;
	bool IsValid() const { return initialized; }
};

void testbed_onsceneload(EngineContext* engine)
{
    GameGlobals& globals = *MENEW(&engine->gameArena, GameGlobals);
	globals.testEntity = Entity::CreateEntity("bruh", meTransform());
	DynArrayPush(engine->sceneSystem->CurrentScene().runtime.entities, globals.testEntity);
	EntityData& entity = Entity::GetEntity(globals.testEntity);
	// Entity::SetFlag(entity, EntityFlags_HIDDEN, true);
	entity.mesh = GenPlaneMesh(2);
}

void testbed_init(EngineContext* engine)
{
    engine->appCallbacks.onSceneLoadFn = testbed_onsceneload;
}

void testbed_update(EngineContext* engine)
{
	GameGlobals& globals = *((GameGlobals*)engine->gameArena.backing_mem);
	ME_ASSERT(globals.IsValid());
	EntityData& entity = Entity::GetEntity(globals.testEntity);
	entity.transform.position.x += sin(GetTimeSec());
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);