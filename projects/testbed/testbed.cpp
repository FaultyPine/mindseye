
#include "mindseye/core/me_app.h"
#include "mindseye/core/me_log.h"
#include "mindseye/scene/me_scene.h"
#include "mindseye/core/me_math.h"
#include "mindseye/render/me_mesh.h"
// BOOKMARK: 
// make "enemy" move toward "crystal"
// make "enemy" attack "crystal" and destroy it (which loses the game) if crystal loses all health
// allow placing simple "trap" which hurts enemy
// implement camera panning/movement (how to design this...?)
// make more things to place (springboard, funnel, mine?)

struct Enemy
{
	EntityRef entity;
};

struct Crystal
{
	EntityRef entity;
};

struct GameGlobals
{
	bool initialized = false;
	Enemy enemies[50];
	Crystal crystal;
	bool IsValid() const { return initialized; }
};

void testbed_init(EngineContext* engine)
{
	GameGlobals& globals = *MENEW(&engine->gameArena, GameGlobals);
	globals.crystal = Crystal { .entity = Entity::CreateEntity("Crystal") };
	meMeshID enemyMesh = GenSphereMesh(3);
	for (s32 i = 0; i < 50; i++)
	{
		StringView entityName = StringFormat("Enemy %i", i);
		Transform tf = Transform(glm::vec3(i * 5.0f, 0, 0));
		globals.enemies[i] = Enemy { .entity = Entity::CreateEntity(entityName.data, tf) };
		EntityData& enemyData = Entity::GetEntity(globals.enemies[i].entity);
		enemyData.mesh = enemyMesh;
	}
	globals.initialized = true;
}

glm::vec3 CalculateAvoidance(meSpanTyped<Enemy> enemies, EntityRef entity)
{
	// TODO: octree
	const EntityData& srcEntityData = Entity::GetEntity(entity);
	glm::vec3 result = glm::vec3(0);
	for (u32 i = 0; i < enemies.size; i++)
	{
		const Enemy& enemy = enemies[i];
		const EntityData& dstEntData = Entity::GetEntity(enemy.entity);
		result += glm::normalize(srcEntityData.transform.position - dstEntData.transform.position);
	}
	result /= enemies.size;
	return result;
}

void testbed_update(EngineContext* engine)
{
	GameGlobals& globals = *((GameGlobals*)engine->gameArena.backing_mem);
	ME_ASSERT(globals.IsValid());
	const EntityData& crystal = Entity::GetEntity(globals.crystal.entity);
	for (u32 i = 0; i < 50; i++)
	{
		Enemy& enemy = globals.enemies[i];
		EntityData& enemyData = Entity::GetEntity(enemy.entity);
		glm::vec3 dir = CalculateAvoidance(meSpanTyped<Enemy>(globals.enemies), enemy.entity);
		enemyData.transform.position += dir;
		enemyData.transform.position += glm::normalize(crystal.transform.position - enemyData.transform.position);
	}
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_MINDSEYE_APP(testbed_init, testbed_update, testbed_shutdown);