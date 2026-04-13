#pragma once

#include "render/me_camera.h"
#include "scene/me_entity.h"
#include "editor/me_asset_editor.h"

struct EditorContext
{
	meCamera editorCamera = {};
	EntityRef selectedEntity = {};
	AssetEditorContext assetEditor = {};
    bool sceneDirty = false;
};

EditorContext& meEditorGetCtx();

void meEditorInitialize(EngineContext* ctx);
void meEditorTick(EngineContext* ctx);
