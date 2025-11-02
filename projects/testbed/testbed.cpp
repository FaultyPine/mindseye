
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"

void testbed_init(EngineContext* engine)
{
	//engine->sceneSystem->rootScene.mainCamera.cameraPos = { 10.0f, 5.0f, 10.0f };
}

void testbed_update(EngineContext* engine)
{
    
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);