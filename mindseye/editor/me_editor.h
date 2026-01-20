#pragma once

#include "render/me_camera.h"

struct EditorContext
{
	meCamera editorCamera = {};
};

EditorContext& meEditorGetCtx();

void meEditorInitialize(EngineContext* ctx);
void meEditorTick(EngineContext* ctx);
