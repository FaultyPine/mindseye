#pragma once

#include "render/me_camera.h"
#include "scene/me_entity.h"
#include "editor/me_asset_editor.h"
#include "core/containers/me_hybrid_array.h"

struct InspectorWindow
{
    meAsset currentAsset = {};
    bool active = false;
};

struct EditorContext
{
	meCamera editorCamera = {};
	HybridArray<InspectorWindow, 4> inspectors = {};
    meTypedAsset<MAEntity> editorSelectedObj = {}; // TODO: will be multiple in the future
	AssetEditorContext assetEditor = {};
    meMap<meAsset, bool> dirtyAssets = {};
};

EditorContext& meEditorGetCtx();

void meEditorInitialize(EngineContext* ctx);
void meEditorTick(EngineContext* ctx);
