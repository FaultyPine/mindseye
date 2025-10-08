
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"

void testbed_init(EngineContext* engine)
{
	StringView sceneFilePath = STRING_LIT("projects/testbed/testscene.scn");
	meScene& testscene = engine->sceneSystem->scene;
	testscene.externalScenePath = STRING_LIT("gltf-samples/Models/BarramundiFish/glTF/BarramundiFish.gltf");
	engine->sceneSystem->WriteSceneToFileBlocking(&testscene, sceneFilePath);
	testscene = {};
	engine->sceneSystem->LoadSceneFromFileBlocking(sceneFilePath, &engine->engineSceneAllocator, &testscene);
}

void testbed_update(EngineContext* engine)
{
    
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_ME_CALLBACKS(AppCallbacks(testbed_init, testbed_update, testbed_shutdown));