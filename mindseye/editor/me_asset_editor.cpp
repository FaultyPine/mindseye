#include "me_asset_editor.h"

#define IMGUI_NODE_EDITOR_API MEAPI
#include "imgui_node_editor.h"
#include "external/bgfx/bgfx/3rdparty/dear-imgui/imgui.h"

#include "core/me_core.h"
#include "core/me_app.h"
#include "core/me_log.h"
#include "core/me_command.h"
#include "scene/me_entity.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"

namespace ed = ax::NodeEditor;

// ID encoding: bases occupy distinct high bits so ranges don't overlap.
// Asset node: ASSET_NODE_BASE + (type << 32) + id

STATIC_ASSERT(sizeof(((MAID*)0)->id)   == sizeof(u32));
STATIC_ASSERT(sizeof(((MAID*)0)->type) == sizeof(u32));
STATIC_ASSERT(NUM_ASSET_TYPES < 256);

static constexpr u64 ASSET_NODE_BASE  = 0x2000000000ULL;
static constexpr float ASSET_NODE_INSPECTOR_WIDTH = 560.0f;
static constexpr float ASSET_NODE_LABEL_WIDTH = 120.0f;

static ed::NodeId MakeAssetNodeId(MAID maid)
{
	return ed::NodeId(ASSET_NODE_BASE + ((u64)maid.type << 32) + (u64)maid.id);
}

static bool AssetEditorHasOpenAsset(const AssetEditorContext& ctx, MAID id)
{
	for (u32 i = 0; i < ctx.openAssetCount; i++)
		if (ctx.openAssets[i].id == id)
			return true;
	return false;
}

static void AssetEditorAddOpenAsset(AssetEditorContext& ctx, meAsset asset)
{
	if (!asset.isValid() || AssetEditorHasOpenAsset(ctx, asset.id)) return;
	if (ctx.openAssetCount >= sizeof(ctx.openAssets) / sizeof(ctx.openAssets[0])) return;

	ctx.openAssets[ctx.openAssetCount++] = asset;
	ctx.needsNavigateToContent = true;
}

static meAsset AssetEditorGetLoadedAsset(MAID id)
{
	meAsset* tmplAsset = meAssetTryGetTemplate(id);
	if (tmplAsset) return *tmplAsset;

	meAssetRequestLoadTemplate(&id, 1);
	tmplAsset = meAssetTryGetTemplate(id);
	return tmplAsset ? *tmplAsset : meAsset(id);
}

static bool DrawAssetNode(meAsset asset, AssetEditorContext& ctx)
{
    meAssetType assetType = asset.id.GetType();
    if (assetType == MABadData || assetType >= NUM_ASSET_TYPES) return false;

    meAssetSystem& sys = meAssetSystemGet();
    meAssetLoader* loader = sys.assetLoaders[assetType];
    if (!loader || !loader->assetTypeDesc || !loader->resourcePool) return false;

    void* dataOpaque = loader->resourcePool->GetOpaque(asset.runtimeHandle);
    if (!dataOpaque) return false;

    u8* dataPtr = (u8*)dataOpaque;
    const meTypeDescriptor& typeDesc = *loader->assetTypeDesc;

    StringView typeName = meAssetTypeToString(assetType);
    StringView path = meAssetIndexGetFilesystemPath(asset.id);

    ed::BeginNode(MakeAssetNodeId(asset.id));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.98f, 0.48f, 1.00f));
    ImGui::Text(ICON_FA_FILE " %s", typeName.cstr());
    ImGui::PopStyleColor();

    if (path.data && path.len)
        ImGui::Text(STRING_FMT, STRING_VAARGS(path));
    else
        ImGui::TextDisabled("(no file)");

    bool changed = false;
    if (ed::IsNodeSelected(MakeAssetNodeId(asset.id)))
    {
        ImGui::Separator();

        if (ImGui::BeginTable("##fields", 2,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
            ImVec2(ASSET_NODE_INSPECTOR_WIDTH, 0.0f)))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, ASSET_NODE_LABEL_WIDTH);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            changed = DrawStructFields(typeDesc, dataPtr, &ctx);
            ImGui::EndTable();
        }

    }

    ed::EndNode();
    return changed;
}

void meAssetEditorInitialize(AssetEditorContext& ctx)
{
	ed::Config config;
	config.SettingsFile = nullptr;
	static const float zoomLevels[] = { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f };
	config.CustomZoomLevels = ImVector<float>();
	config.CustomZoomLevels.resize(sizeof(zoomLevels) / sizeof(zoomLevels[0]));
	memcpy(config.CustomZoomLevels.Data, zoomLevels, sizeof(zoomLevels));
	ctx.nodeEditorCtx = ed::CreateEditor(&config);
}

void meAssetEditorShutdown(AssetEditorContext& ctx)
{
	if (ctx.nodeEditorCtx)
	{
		ed::DestroyEditor(ctx.nodeEditorCtx);
		ctx.nodeEditorCtx = nullptr;
	}
}

void meAssetEditorOpen(AssetEditorContext& ctx, meAsset asset)
{
	ctx.rootAsset = asset;
	ctx.openAssetCount = 0;
	AssetEditorAddOpenAsset(ctx, asset);
	ctx.isOpen = true;
	ctx.needsNavigateToContent = true;
}

bool meAssetEditorTick(AssetEditorContext& ctx)
{
	if (!ctx.isOpen) return false;

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(ICON_FA_DIAGRAM_PROJECT " Asset Editor###Asset Editor", &ctx.isOpen))
	{
		ImGui::End();
		return false;
	}

    meAsset& asset = ctx.rootAsset;
	if (!asset.isValid())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("No entity selected.");
		ImGui::PopStyleColor();
		ImGui::End();
		return false;
	}

    meMap<meAsset, bool>& dirtyAssets = ctx.dirtyAssets;
	bool hasDirtyAssets = false;
	for (auto& [dirtyAsset, dirty] : dirtyAssets)
		if (dirty) { hasDirtyAssets = true; break; }

	if (hasDirtyAssets)
	{
		if (ImGui::Button("Save All"))
		{
            // TOOD: parallel
			for (auto& [dirtyAsset, dirty] : dirtyAssets)
			{
				if (!dirty) continue;
				meExternalCommand cmd = {};
				cmd.type = meExternalCommandType_SaveAsset;
				cmd.saveAsset.asset = dirtyAsset;
				meSendExternalCommand(cmd);
				dirty = false;
			}
		}
		ImGui::Separator();
	}

	ed::SetCurrentEditor(ctx.nodeEditorCtx);
	ed::Begin("AssetEditor");

	bool anyChanged = false;
	bool rootChanged = false;

	for (u32 i = 0; i < ctx.openAssetCount; i++)
	{
		meAsset& openAsset = ctx.openAssets[i];
		if (!openAsset.isLoaded())
		{
			meAsset* loadedAsset = meAssetTryGetTemplate(openAsset.id);
			if (loadedAsset)
				openAsset = *loadedAsset;
		}

		bool assetChanged = DrawAssetNode(openAsset, ctx);
		anyChanged |= assetChanged;
		if (assetChanged)
		{
			ctx.dirtyAssets[openAsset] = true;
			if (openAsset.id == asset.id)
				rootChanged = true;
		}
	}

	if (ctx.needsNavigateToContent)
	{
		ed::NavigateToContent();
		ctx.needsNavigateToContent = false;
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);

	if (rootChanged)
		ctx.dirtyAssets[asset] = true;

    ctx.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ImGui::End();
	return anyChanged;
}







// Generic editor type drawing


static void InspectorLabel(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

bool DrawAssetField(
    const meTypeDescriptor& field,
    MAID* maid,
	AssetEditorContext* ctx)
{
    if (!maid)
    {
        return false;
    } 
    bool changed = false;
    const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();
    // For meTypedAsset<T> fields the reflector stores the enum value as an integral stub
    // in field.templatedTypes[0]. Prefer that over the MAID's stored type so that the
    // combo is correctly filtered even when the MAID is default-constructed ("none").
    meAssetType assetType = (field.templatedTypes.size > 0)
        ? (meAssetType)field.templatedTypes[0]->value
        : maid->GetType();

    if (assetType <= MABadData || assetType >= NUM_ASSET_TYPES)
    {
        return false;
    }

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(displayName);
	if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);

    StringView currentPath = meAssetIndexGetFilesystemPath(*maid);
    const char* preview = (currentPath.data && currentPath.len)
        ? currentPath.cstr()
        : ((*maid) ? "(unknown asset)" : "(none)");

	float openButtonWidth = ctx ? ImGui::GetFrameHeight() : 0.0f;
	float spacing = ctx ? ImGui::GetStyle().ItemInnerSpacing.x : 0.0f;
	float comboWidth = ImGui::GetContentRegionAvail().x - openButtonWidth - spacing;
	if (comboWidth < 60.0f) comboWidth = 60.0f;

	ImGui::PushItemWidth(comboWidth);
	bool popupOpened = false;
	if (ImGui::Button(preview, ImVec2(comboWidth, 0.0f)))
	{
		ImGui::OpenPopup("##asset_picker");
		popupOpened = true;
	}
	ImGui::PopItemWidth();

	ImVec2 popupPos = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
	bool popupOpen = popupOpened || ImGui::IsPopupOpen("##asset_picker");
	if (ctx && popupOpen)
		ed::Suspend();

	if (popupOpen)
	{
		ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(comboWidth, 0.0f), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("##asset_picker"))
		{
			if (ImGui::Selectable("(none)", !(*maid)))
			{
				maid->SetID(U32_INVALID_ID);
				changed = true;
			}

			const meAssetIndex& index = meAssetIndexGetRO();
			for (auto& [id, path] : index.assetToPathMap)
			{
				if (id.GetType() != assetType || !path) continue;

				bool isSelected = (*maid == id);
				if (ImGui::Selectable(path.cstr(), isSelected))
				{
					*maid = id;
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndPopup();
		}
	}

	if (ctx && popupOpen)
		ed::Resume();

	if (ctx)
	{
		ImGui::SameLine();
		bool canOpen = *maid;
		if (!canOpen)
			ImGui::BeginDisabled();

		if (ImGui::SmallButton("+"))
		{
			meAsset referencedAsset = AssetEditorGetLoadedAsset(*maid);
			AssetEditorAddOpenAsset(*ctx, referencedAsset);
		}

		if (!canOpen)
			ImGui::EndDisabled();
	}
    return changed;
}

bool DrawPrimitiveValue(const meTypeDescriptor& type, u8* data, const meTypeDescriptor* parentType, AssetEditorContext* ctx)
{
	bool changed = false;

    static thread_local char s_textEditIntermediateBuf[75] = {};
	if (&type == &TD_FLOAT)
	{
		changed = ImGui::DragFloat("##v", (float*)data, 0.01f);
	}
	else if (&type == &TD_DOUBLE)
	{
		float tmp = (float)*(double*)data;
		if (ImGui::DragFloat("##v", &tmp, 0.01f)) { *(double*)data = tmp; changed = true; }
	}
	else if (&type == &TD_INT)
	{
		changed = ImGui::DragInt("##v", (int*)data);
	}
	else if (&type == &TD_UNSIGNED_INT)
	{
		changed = ImGui::DragScalar("##v", ImGuiDataType_U32, data);
	}
	else if (&type == &TD_SHORT)
	{
		changed = ImGui::DragScalar("##v", ImGuiDataType_S16, data);
	}
	else if (&type == &TD_UNSIGNED_SHORT)
	{
		changed = ImGui::DragScalar("##v", ImGuiDataType_U16, data);
	}
	else if (&type == &TD_LONG || &type == &TD_LONGLONG)
	{
		changed = ImGui::DragScalar("##v", ImGuiDataType_S64, data);
	}
	else if (&type == &TD_UNSIGNED_LONG || &type == &TD_UNSIGNED_LONG_LONG)
	{
		changed = ImGui::DragScalar("##v", ImGuiDataType_U64, data);
	}
	else if (&type == &TD_BOOL)
	{
		changed = ImGui::Checkbox("##v", (bool*)data);
	}
	else if (&type == &TD_CHAR || &type == &TD_UNSIGNED_CHAR)
	{
		int tmp = *(u8*)data;
		if (ImGui::DragInt("##v", &tmp, 1.0f, 0, 255)) { *(u8*)data = (u8)tmp; changed = true; }
	}
	else if (&type == &TD_VEC3)
	{
		changed = ImGui::DragFloat3("##v", (float*)data, 0.01f);
	}
	else if (&type == &TD_QUAT)
	{
		glm::quat& q = *(glm::quat*)data;
		glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
		if (ImGui::DragFloat3("##v", &euler.x, 0.5f))
		{
			q = glm::quat(glm::radians(euler));
			changed = true;
		}
	}
    else if (&type == &TD_STRING)
	{
		String* str = (String*)data;
		s_textEditIntermediateBuf[0] = '\0';
		if (str->data && str->len)
		{
			u32 copyLen = (u32)Math::Min(str->len, (u64)IM_ARRAYSIZE(s_textEditIntermediateBuf) - 1);
			ME_MEMCPY(s_textEditIntermediateBuf, str->data, copyLen);
			s_textEditIntermediateBuf[copyLen] = '\0';
		}
        if (ImGui::InputTextWithHint("##v", "Enter text here...", s_textEditIntermediateBuf, IM_ARRAYSIZE(s_textEditIntermediateBuf)))
		{
			*str = StringFromCString(s_textEditIntermediateBuf);
			changed = true;
		}
	}
	else if (&type == &TD_STRINGVIEW)
	{
		StringView* sv = (StringView*)data;
		s_textEditIntermediateBuf[0] = '\0';
		if (sv->data && sv->len)
		{
			u32 copyLen = (u32)Math::Min(sv->len, (u64)IM_ARRAYSIZE(s_textEditIntermediateBuf) - 1);
			ME_MEMCPY(s_textEditIntermediateBuf, sv->data, copyLen);
			s_textEditIntermediateBuf[copyLen] = '\0';
		}
		if (ImGui::InputTextWithHint("##v", "Enter text here...", s_textEditIntermediateBuf, IM_ARRAYSIZE(s_textEditIntermediateBuf)))
		{
			*sv = StringFormatNew(GetDefaultAllocator(), "%s", s_textEditIntermediateBuf);
			changed = true;
		}
	}
	else if (type.iterateContentFn)
	{
		if (!parentType) return false;

		struct DrawCtx
		{
			const meTypeDescriptor* parentType;
			AssetEditorContext* editorCtx;
			bool           changed   = false;
			bool           doRemove  = false;
			meContainerKey removeKey = 0;
		};
		DrawCtx drawCtx = { parentType, ctx };

		float tableWidth = ImGui::GetContentRegionAvail().x;
		if (tableWidth < 120.0f)
			tableWidth = 120.0f;

		if (ImGui::BeginTable("##container_table", 2,
			ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			ImVec2(tableWidth, 0.0f)))
		{
			ImGui::TableSetupColumn("#",     ImGuiTableColumnFlags_WidthFixed, 40.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			type.iterateContentFn(data, parentType,
				+[](void* elemPtr, const meTypeDescriptor* elemType, meContainerKey elemKey, void* userData)
				{
					DrawCtx& ctx = *(DrawCtx*)userData;

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%llu", (unsigned long long)elemKey);
					ImGui::SameLine();
					ImGui::PushID((int)elemKey);
					if (ImGui::SmallButton("-") && !ctx.doRemove)
					{
						ctx.doRemove  = true;
						ctx.removeKey = elemKey;
					}
					ImGui::PopID();

					ImGui::TableSetColumnIndex(1);
					ImGui::PushID((int)elemKey);
					ctx.changed |= DrawPrimitiveValue(*elemType, (u8*)elemPtr, ctx.parentType, ctx.editorCtx);
					ImGui::PopID();
				},
				&drawCtx);

			ImGui::EndTable();
		}

		// Defer removal until after iteration so the array stays valid while we draw.
		if (drawCtx.doRemove && type.removeElementFn)
		{
			type.removeElementFn(data, parentType, drawCtx.removeKey);
			drawCtx.changed = true;
		}

		if (type.pushElementFn && ImGui::Button("Add"))
		{
			if (parentType->templatedTypes)
			{
				const meTypeDescriptor* elemType = parentType->templatedTypes[0];
				if (elemType)
				{
					Allocation elementData = MECALLOC(GetTLScratch(), elemType->size);
					if (elemType->setToDefaultsFn) elemType->setToDefaultsFn(elementData);
					type.pushElementFn(data, parentType, elementData);
					drawCtx.changed = true;
				}
			}
		}

		return drawCtx.changed;
	}
    else if (&type == &TD_MEASSET || type.thisType == &TD_MEASSET)
    {
        // Matches both TD_MEASSET directly and intermediate typed-asset descriptors
        // (e.g. g_typearg_entities_0 for DynArray<meTypedAsset<MAEntity>> elements),
        // which have .thisType == &TD_MEASSET and carry the enum value in templatedTypes[0].
        meAsset& asset = *(meAsset*)data;
        MAID& maid = asset.id;
	    changed = DrawAssetField(type, &maid, ctx);
    }
    else if (&type == &TD_MAID)
    {
        MAID* maid = (MAID*)data;
        changed = DrawAssetField(type, maid, ctx);
    }
	else
	{
		return false; // not a primitive we handle
	}
	return changed;
}

bool DrawStructFields(const meTypeDescriptor& type, u8* dataPtr, AssetEditorContext* ctx)
{
	bool anyChanged = false;
	for (u32 i = 0; i < type.fields.size; i++)
	{
		const meTypeDescriptor& field = type.fields[i];
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_PaddingMember)) continue;
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_Excluded)) continue;

		ImGui::PushID((int)i);
		if (DrawTypeDescriptorField(field, dataPtr, ctx))
			anyChanged = true;
		ImGui::PopID();
	}
	return anyChanged;
}


// dataPtr is the base address of the parent struct
bool DrawTypeDescriptorField(const meTypeDescriptor& field, u8* dataPtr, AssetEditorContext* ctx)
{
	u8* fieldData = dataPtr + (field.offsetBits / 8);
	const meTypeDescriptor* fieldType = field.thisType;
	bool changed = false;

	if (!fieldType) return false;

	const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();

	if (fieldType == &TD_MEASSET || fieldType->thisType == &TD_MEASSET)
	{
		meAsset& asset = *(meAsset*)fieldData;
		changed = DrawAssetField(field, &asset.id, ctx);
	}
	else if (fieldType == &TD_MAID)
	{
		changed = DrawAssetField(field, (MAID*)fieldData, ctx);
	}
	else if (fieldType->fields.size > 0)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();

		bool nodeOpen = ImGui::TreeNodeEx(displayName,
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen);

        if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
		}
		ImGui::TableSetColumnIndex(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(STRING_FMT, STRING_VAARGS(fieldType->name));
		ImGui::PopStyleColor();

		if (nodeOpen)
		{
			changed = DrawStructFields(*fieldType, fieldData, ctx);
			ImGui::TreePop();
		}
	}
	else
	{
		InspectorLabel(displayName);
		ImGui::PushID(displayName);
		changed = DrawPrimitiveValue(*fieldType, fieldData, &field, ctx);
		ImGui::PopID();

		if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
		}
	}
	return changed;
}

