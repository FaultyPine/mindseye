#pragma once

#include "scene/me_entity.h"
#include "asset/me_asset.h"
#include "core/containers/me_map.h"

namespace ax { namespace NodeEditor { struct EditorContext; }}

struct AssetEditorContext
{
	ax::NodeEditor::EditorContext* nodeEditorCtx = nullptr;
	meAsset rootAsset = {};
	meAsset openAssets[16] = {};
	u32 openAssetCount = 0;
	bool isOpen = false;
    bool isFocused = false;
	bool needsNavigateToContent = false;
    meMap<meAsset, bool> dirtyAssets = {};
};

void meAssetEditorInitialize(AssetEditorContext& ctx);
void meAssetEditorShutdown(AssetEditorContext& ctx);
void meAssetEditorOpen(AssetEditorContext& ctx, meAsset asset);
bool meAssetEditorTick(AssetEditorContext& ctx);

// Generic editor type drawing

bool DrawTypeDescriptorField(
    const meTypeDescriptor& field, 
    u8* dataPtr,
	AssetEditorContext* ctx = nullptr);

bool DrawStructFields(
    const meTypeDescriptor& type, 
    u8* dataPtr,
	AssetEditorContext* ctx = nullptr);

bool DrawPrimitiveValue(
    const meTypeDescriptor& type, 
    u8* data, 
    const meTypeDescriptor* parentType = nullptr,
	AssetEditorContext* ctx = nullptr);

bool DrawAssetField(
    const meTypeDescriptor& field,
    MAID* maid,
	AssetEditorContext* ctx = nullptr);
