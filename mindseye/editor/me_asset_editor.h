#pragma once

#include "scene/me_entity.h"
#include "asset/me_asset.h"

namespace ax { namespace NodeEditor { struct EditorContext; }}

struct AssetEditorPendingLink
{
	bool active = false;
	uintptr_t sourcePinId = 0;
	EntityRef sourceEntity = {};
	u32 sourceFieldIndex = 0;
	MAID* sourceMaidPtr = nullptr;
	meAssetType expectedType = MABadData;
};

struct AssetEditorContext
{
	ax::NodeEditor::EditorContext* nodeEditorCtx = nullptr;
	EntityRef rootEntity = {};
	bool isOpen = false;
    bool isFocused = false;
	bool needsNavigateToContent = false;
	AssetEditorPendingLink pendingLink = {};
	char searchBuf[256] = {};
};

void meAssetEditorInitialize(AssetEditorContext& ctx);
void meAssetEditorShutdown(AssetEditorContext& ctx);
void meAssetEditorOpen(AssetEditorContext& ctx, EntityRef entity);
void meAssetEditorTick(AssetEditorContext& ctx);
