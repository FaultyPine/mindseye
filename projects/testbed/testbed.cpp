
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
    
}

void testbed_update(EngineContext* engine)
{
    auto& entities = engine->sceneSystem->CurrentScene().entities;
    for (DynArray_Foreach(entities, i))
    {
        auto& entityAsset = entities[i];
        ScopedAssetLockW<meEntity> entity(entityAsset);
        if (entity)
        {
            //entity->transform.position.x = sinf(GetTimeSec() * 1.5f) * 10.0f;
        }
    }
}

void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);
