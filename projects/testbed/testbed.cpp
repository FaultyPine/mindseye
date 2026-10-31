#include "mindseye/core/me_app.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/render/me_mesh.h"
#include "mindseye/editor/me_editor.h"

namespace
{

meMeshID MakeMesh(void (*generate)(meMeshID, u32, meMaterialID),
    meMaterialID material)
{
    meMeshID handle = meMeshPoolGet().Load({.resourceType = meResourceType_InstanceAsset});
    generate(handle, 32, material);
    return handle;
}

void Add(meScene& scene, StringView name, meMeshID mesh,
    glm::vec3 position, glm::vec3 scale, float tilt = 0)
{
    auto asset = meEntityCreateBlankInstance(name);
    meEntity& entity = meEntityGet(asset);
    entity.mesh = meTypedAsset<MAMesh>(mesh);
    glm::quat rotation = glm::angleAxis(glm::radians(tilt), glm::vec3(0, 0, 1));
    entity.transform = meTransform(position, scale, rotation);
    BoundingBox bounds = meMeshPoolGet().Get(mesh).meshBounds;
    entity.authoritativeBoundsOS = bounds;
    DynArrayPush(scene.entities, asset);
}

void BuildPlayground(EngineContext* engine)
{
    meScene& scene = engine->sceneSystem->CurrentScene();
    if (DynArrayGetSize(scene.entities))
        return;

    auto color = [](StringView name, float r, float g, float b) 
    {
        return meMaterialCreateDefault(name, glm::vec3(r, g, b));
    };
    meMaterialID sand = color(STRING_LIT("Warm limestone"), 0.66f, 0.61f, 0.49f);
    meMaterialID cream = color(STRING_LIT("Chalk"), 0.88f, 0.83f, 0.69f);
    meMaterialID teal = color(STRING_LIT("Workshop teal"), 0.16f, 0.49f, 0.46f);
    meMaterialID orange = color(STRING_LIT("Safety orange"), 0.88f, 0.38f, 0.18f);
    meMaterialID dark = color(STRING_LIT("Blue charcoal"), 0.17f, 0.23f, 0.27f);
    meMaterialID gold = color(STRING_LIT("Golden yellow"), 0.96f, 0.69f, 0.25f);
    meMaterialID green = color(STRING_LIT("Sage"), 0.39f, 0.52f, 0.38f);

    meMeshID floor = MakeMesh(GenPlaneMesh, sand);
    meMeshID chalkBox = MakeMesh(GenCubeMesh, cream);
    meMeshID tealBox = MakeMesh(GenCubeMesh, teal);
    meMeshID orangeBox = MakeMesh(GenCubeMesh, orange);
    meMeshID darkBox = MakeMesh(GenCubeMesh, dark);
    meMeshID goldBox = MakeMesh(GenCubeMesh, gold);
    meMeshID sageBox = MakeMesh(GenCubeMesh, green);
    meMeshID ball = MakeMesh(GenSphereMesh, orange);
    meMeshID chalkBall = MakeMesh(GenSphereMesh, cream);
    meMeshID cylinder = MakeMesh(GenCylinderMesh, teal);
    meMeshID goldCylinder = MakeMesh(GenCylinderMesh, gold);

    // generated
    Add(scene, STRING_LIT("Courtyard"), floor, {0, 0, 0}, {32, 1, 28});
    Add(scene, STRING_LIT("Foundation"), darkBox, {0, -0.35f, 0}, {32.4f, 0.6f, 28.4f});
    for (int side = -1; side <= 1; side += 2)
    {
        Add(scene, STRING_LIT("Side curb"), chalkBox, {side * 15.8f, 0.2f, 0}, {0.4f, 0.4f, 28});
        Add(scene, STRING_LIT("End curb"), chalkBox, {0, 0.2f, side * 13.8f}, {32, 0.4f, 0.4f});
    }
    for (int i = 0; i < 9; ++i)
        Add(scene, STRING_LIT("Approach marker"), chalkBox, {0, 0.015f, 10.0f - i * 2}, {0.18f, 0.02f, 0.8f});
    Add(scene, STRING_LIT("Base plinth"), chalkBox, {0, 0.3f, -9}, {5, 0.6f, 4});
    Add(scene, STRING_LIT("Base tower"), cylinder, {0, 2.0f, -9}, {1.2f, 2.8f, 1.2f});
    Add(scene, STRING_LIT("Base collar"), goldCylinder, {0, 3.4f, -9}, {1.45f, 0.25f, 1.45f});
    Add(scene, STRING_LIT("Base beacon"), chalkBall, {0, 4.1f, -9}, {0.65f, 0.65f, 0.65f});

    Add(scene, STRING_LIT("Ramp"), tealBox, {-8, 1.0f, 3}, {5, 0.35f, 2.8f}, -20);
    Add(scene, STRING_LIT("Ramp support"), darkBox, {-9.7f, 0.65f, 3}, {0.5f, 1.3f, 2.2f});
    Add(scene, STRING_LIT("Starter ball"), ball, {-9.5f, 2.4f, 3}, {0.7f, 0.7f, 0.7f});
    for (int i = 0; i < 6; ++i)
        Add(scene, STRING_LIT("Domino"), i % 2 ? goldBox : orangeBox,
            {-4.5f + i * 0.9f, 0.75f, 3}, {0.24f, 1.5f, 0.9f});

    for (int side = -1; side <= 1; side += 2)
    {
        Add(scene, STRING_LIT("Pendulum foot"), chalkBox, {7 + side * 2.2f, 0.15f, 1}, {1, 0.3f, 2});
        Add(scene, STRING_LIT("Pendulum upright"), darkBox, {7 + side * 2.2f, 2.6f, 1}, {0.3f, 5.2f, 0.3f});
    }
    Add(scene, STRING_LIT("Pendulum crossbar"), tealBox, {7, 5.1f, 1}, {5, 0.35f, 0.5f});
    Add(scene, STRING_LIT("Pendulum rod"), darkBox, {7, 3.65f, 1}, {0.10f, 2.6f, 0.10f});
    Add(scene, STRING_LIT("Pendulum weight"), ball, {7, 2, 1}, {0.85f, 0.85f, 0.85f});

    Add(scene, STRING_LIT("Shape study plinth"), chalkBox, {-8, 0.2f, -6.5f}, {7, 0.4f, 3});
    Add(scene, STRING_LIT("Study cube"), orangeBox, {-10, 1.2f, -6.5f}, {1.6f, 1.6f, 1.6f});
    Add(scene, STRING_LIT("Study sphere"), chalkBall, {-7.8f, 1.3f, -6.5f}, {0.9f, 0.9f, 0.9f});
    Add(scene, STRING_LIT("Study cylinder"), cylinder, {-5.6f, 1.4f, -6.5f}, {0.7f, 2, 0.7f});
    for (int i = 0; i < 3; ++i)
        Add(scene, STRING_LIT("Stepped platform"), sageBox, {8, 0.3f + i * 0.3f, -6.0f - i * 1.4f}, {4, 0.6f + i * 0.6f, 1.4f});
}
}

void testbed_init(EngineContext* engine)
{
    BuildPlayground(engine);
}

void testbed_update(EngineContext* engine) {}
void testbed_shutdown(EngineContext* engine) {}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);
