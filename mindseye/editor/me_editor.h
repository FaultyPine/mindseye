#pragma once

#include "render/me_camera.h"
#include "scene/me_entity.h"

struct EditorContext
{
	meCamera editorCamera = {};
	EntityRef selectedEntity = {};
};

EditorContext& meEditorGetCtx();

void meEditorInitialize(EngineContext* ctx);
void meEditorTick(EngineContext* ctx);
