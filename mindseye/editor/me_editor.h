#pragma once

#include "render/me_camera.h"
#include "scene/me_entity.h"
#include "editor/me_asset_editor.h"
#include "core/containers/me_hybrid_array.h"

struct InspectorWindow
{
    MAID currentAsset = MAID_INVALID;
    bool active = false;
};

struct EditorContext
{
	meCamera editorCamera = {};
	HybridArray<InspectorWindow, 4> inspectors = {};
    EntityRef selectedEntity = {}; // TODO: will be multiple in the future
	AssetEditorContext assetEditor = {};
    meMap<MAID, bool> dirtyAssets = {};
};

EditorContext& meEditorGetCtx();

void meEditorInitialize(EngineContext* ctx);
void meEditorTick(EngineContext* ctx);
