
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"

void testbed_init(EngineContext* engine)
{
	StringView sceneFilePath = STRING_LIT("testscene.scn");
	meScene& testscene = engine->sceneSystem->scene;
	engine->sceneSystem->LoadSceneFromFileBlocking(sceneFilePath, &engine->engineSceneAllocator, &testscene);
}

void testbed_update(EngineContext* engine)
{
    
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);