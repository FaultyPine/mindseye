#pragma once

#include "scene/me_entity.h"
#include "asset/me_asset.h"
#include "core/containers/me_map.h"

namespace ax { namespace NodeEditor { struct EditorContext; }}

struct AssetEditorPendingLink
{
	bool active = false;
	uintptr_t sourcePinId = 0;
	meAsset sourceAsset = {};
	u32 sourceFieldIndex = 0;
	meAssetType expectedType = MABadData;
};

struct AssetEditorContext
{
	ax::NodeEditor::EditorContext* nodeEditorCtx = nullptr;
	meAsset rootAsset = {};
	bool isOpen = false;
    bool isFocused = false;
	bool needsNavigateToContent = false;
	AssetEditorPendingLink pendingLink = {};
	char searchBuf[256] = {};
    meMap<meAsset, bool> dirtyAssets = {};
};

void meAssetEditorInitialize(AssetEditorContext& ctx);
void meAssetEditorShutdown(AssetEditorContext& ctx);
void meAssetEditorOpen(AssetEditorContext& ctx, meAsset asset);
bool meAssetEditorTick(AssetEditorContext& ctx);

// Generic editor type drawing

bool DrawTypeDescriptorField(
    const meTypeDescriptor& field, 
    u8* dataPtr);

bool DrawStructFields(
    const meTypeDescriptor& type, 
    u8* dataPtr);

bool DrawPrimitiveValue(
    const meTypeDescriptor& type, 
    u8* data, 
    const meTypeDescriptor* parentType = nullptr);

bool DrawAssetField(
    const meTypeDescriptor& field,
    MAID* maid);