#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "scene/me_entity.h"
#include "render/me_camera.h"
#include "core/containers/dynarray.h"
#include "asset/me_asset.h"
#ifndef ME_REFLECTING
#include "generatedtypes/me_scene.generated.h"
#endif

typedef meResource meSceneID;

struct MEREFLECT(type, Description="Scene Description")
meScene : public meBaseAsset
{
    using meBaseAsset::meBaseAsset;

	meCamera mainCamera = {};

	DynArray<meTypedAsset<MAEntity>> entities = {};

    void Destroy();
};

struct meSceneRaycastHit
{
	meTypedAsset<MAEntity> entity = {};
	f32 distance = 0.0f;
	glm::vec3 point = glm::vec3(0);
	explicit operator bool() const { return entity != ME_RESOURCE_INVALID; }
};

MEAPI meSceneRaycastHit meSceneRaycast(meScene& scene, const meRay& ray);

struct meSceneManager
{
	MEAPI void ChangeCurrentSceneAsync(
		StringView filename);

	// -------- engine internal --------------------------
	void UnloadCurrentScene();
	void Tick(EngineContext* ctx);
	void CopyToRenderInput(meScene& outScene, meAllocator* frameAllocator);

	meAsset rootScene = {};

	MEAPI meScene& CurrentScene();
};


struct meScenePool : public meResourcePool<meScene>
{
	meScenePool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
		meResourcePool<meScene>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MAScene;
	}

	meResource Load(meResourceCreateParams createParams) override
	{
		meResource e = meResourcePool<meScene>::Load(createParams);
		meScene& scene = Get(e);
		scene.entities = DynArrayCreate<meTypedAsset<MAEntity>>(GetPayloadAllocator());
		return e;
	}
};

meScenePool& meScenePoolGet();

void meSceneInitialize(EngineContext* ctx);
void meSceneInitializeLate(EngineContext* engine);

