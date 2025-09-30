
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"

void testbed_init(EngineContext* engine)
{
	meScene testscene;
	meSceneLoadFromGLTF(&engine->engineSceneAllocator, STRING_LIT("BarramundiFish/glTF/BarramundiFish.gltf"), testscene);
	engine->sceneSystem->scene = testscene;
}

void testbed_update(EngineContext* engine)
{
    
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_ME_CALLBACKS(AppCallbacks(testbed_init, testbed_update, testbed_shutdown));