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
	ctx.expandedAssets[asset.id] = true;
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

    ScopedAssetOpaqueLockW lockedAsset(asset);
    meAssetLoader* loader = lockedAsset.Loader();
    if (!lockedAsset || !loader || !loader->assetTypeDesc) return false;

    void* dataOpaque = lockedAsset.Get();
    if (!dataOpaque) return false;

    u8* dataPtr = (u8*)dataOpaque;
    const meTypeDescriptor& typeDesc = *loader->assetTypeDesc;

    StringView typeName = meAssetTypeToString(assetType);
    StringView path = meAssetIndexGetFilesystemPath(asset.id);

    ed::BeginNode(MakeAssetNodeId(asset.id));
    ImGui::PushID((int)(u64)asset.id);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.98f, 0.48f, 1.00f));
    bool& expanded = ctx.expandedAssets[asset.id];
    if (ImGui::SmallButton(expanded ? "-" : "+"))
        expanded = !expanded;
    ImGui::SameLine();
    ImGui::Text(ICON_FA_FILE " %s", typeName.cstr());
    ImGui::PopStyleColor();

    if (path.data && path.len)
        ImGui::Text(STRING_FMT, STRING_VAARGS(path));
    else
        ImGui::TextDisabled("(no file)");

    bool changed = false;
    if (expanded)
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

    ImGui::PopID();
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
	ctx.expandedAssets.clear();
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


static bool ResetTypedAssetToDefault(const meTypeDescriptor& type, void* data)
{
	if (!(type.thisType == &TD_MEASSET && type.templatedTypes.size > 0 && type.templatedTypes[0]))
		return false;

	meAsset& asset = *(meAsset*)data;
	asset.ResetReferenceToType((meAssetType)type.templatedTypes[0]->value);
	return true;
}

static bool ResetValueWithOwnDefault(const meTypeDescriptor& type, void* data)
{
	if (!data || type.size == 0) return true;

    if (ResetTypedAssetToDefault(type, data))
    {
        return true;
    }

	if (type.setToDefaultsFn)
	{
		type.setToDefaultsFn(data);
		return true;
	}

	if (type.thisType && type.thisType->setToDefaultsFn)
	{
		type.thisType->setToDefaultsFn(data);
		return true;
	}

	return false;
}

static bool ResetValueFromParentDefault(
	const meTypeDescriptor& field,
	void* fieldData,
	const meTypeDescriptor* parentType)
{
	if (!fieldData || !parentType || !parentType->setToDefaultsFn)
		return false;

	if (field.offsetBits < 0 || (field.offsetBits % 8) != 0)
		return false;

	u64 fieldOffset = (u64)field.offsetBits / 8;
	if (fieldOffset + field.size > parentType->size)
		return false;

	Allocation parentData = MECALLOC(GetTLScratch(), parentType->size);
	parentType->setToDefaultsFn(parentData);
	void* defaultFieldData = (u8*)parentData.data + fieldOffset;
	ME_MEMCPY(fieldData, defaultFieldData, field.size);
	return true;
}

static void ResetValueToDefault(
	const meTypeDescriptor& type,
	void* data,
	const meTypeDescriptor* parentType)
{
	if (!data || type.size == 0) return;

    // Prefer parent defaults for fields: a child type may have different defaults
    // when it appears inside a containing type EX: meTransform::scale
	if (ResetValueFromParentDefault(type, data, parentType))
		return;

	if (ResetValueWithOwnDefault(type, data))
		return;

	if (type.thisType && TEST_BIT(type.flags, meTypeDescriptorFlag_ConstantArray))
	{
		meTypeDescriptorWalkElements(type, data,
			[&](const meTypeDescriptorMember& element)
			{
				if (!ResetTypedAssetToDefault(type, element.data))
					ResetValueToDefault(element.field, element.data, nullptr);
				return true;
			});
		return;
	}

    LOG_WARN("Type without a setToDefaultsFn, this isn't expected. " STRING_FMT, STRING_VAARGS(type.name));
	ME_MEMCLEAR(data, type.size);
}

static bool DrawTypeDescriptorFieldContextMenu(
	const char* popupId,
	const meTypeDescriptor& field,
	void* fieldData,
	const meTypeDescriptor* parentType,
	AssetEditorContext* ctx)
{
	if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		ImGui::OpenPopup(popupId);

	bool changed = false;
	bool popupOpen = ImGui::IsPopupOpen(popupId);
	if (ctx && popupOpen)
		ed::Suspend();

	if (ImGui::BeginPopup(popupId))
	{
		if (ImGui::MenuItem("Reset to Default"))
		{
			ResetValueToDefault(field, fieldData, parentType);
			changed = true;
		}
		ImGui::EndPopup();
	}

	if (ctx && popupOpen)
		ed::Resume();
	return changed;
}

static bool InspectorLabel(
	const meTypeDescriptor& field,
	void* fieldData,
	const meTypeDescriptor* parentType,
	AssetEditorContext* ctx)
{
	const char* label = field.editorName.data ? field.editorName.cstr() : field.name.cstr();
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
	bool changed = DrawTypeDescriptorFieldContextMenu("##field_context_label", field, fieldData, parentType, ctx);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	return changed;
}

bool DrawAssetField(
    const meTypeDescriptor& field,
    meAsset* asset,
	const meTypeDescriptor* parentType,
	AssetEditorContext* ctx)
{
    MAID& maid = asset->id;
    bool changed = false;
    // For meTypedAsset<T> fields the reflector stores the enum value as an integral stub
    // in field.templatedTypes[0]. Prefer that over the MAID's stored type so that the
    // combo is correctly filtered even when the MAID is default-constructed ("none").
    meAssetType assetType = (field.templatedTypes.size > 0)
        ? (meAssetType)field.templatedTypes[0]->value
        : maid.GetType();

    if (assetType <= MABadData || assetType >= NUM_ASSET_TYPES)
    {
        return false;
	}

	changed |= InspectorLabel(field, asset, parentType, ctx);

    StringView currentPath = meAssetIndexGetFilesystemPath(maid);
    const char* preview = (currentPath.data && currentPath.len)
        ? currentPath.cstr()
        : (maid ? "(unknown asset)" : "(none)");

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
			if (ImGui::Selectable("(none)", !maid))
			{
				asset->ResetReferenceToType(assetType);
				changed = true;
			}

			const meAssetIndex& index = meAssetIndexGetRO();
			for (auto& [id, path] : index.assetToPathMap)
			{
				if (id.GetType() != assetType || !path) continue;

				bool isSelected = (maid == id);
				if (ImGui::Selectable(path.cstr(), isSelected))
				{
					asset->SetReference(id);
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
		bool canOpen = maid;
		if (!canOpen)
			ImGui::BeginDisabled();

		if (ImGui::SmallButton("+"))
		{
            // if we're drawing a loaded asset, it could be an instance asset
            // if it's unloaded, it's likely just a reference to a template asset
			meAsset referencedAsset = (asset->isLoaded() && asset->id == maid)
				? *asset
				: AssetEditorGetLoadedAsset(maid);
			AssetEditorAddOpenAsset(*ctx, referencedAsset);
		}

		if (!canOpen)
			ImGui::EndDisabled();
	}
	changed |= DrawTypeDescriptorFieldContextMenu("##field_context_value", field, asset, parentType, ctx);
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
	else if (&type == &TD_LONG || &type == &TD_LONG_LONG)
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

			meTypeDescriptorWalkElements(type, data,
				[&](const meTypeDescriptorMember& element)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%llu", (unsigned long long)element.key);
					ImGui::SameLine();
					ImGui::PushID((int)element.key);
					if (ImGui::SmallButton("-") && !drawCtx.doRemove)
					{
						drawCtx.doRemove  = true;
						drawCtx.removeKey = element.key;
					}
					ImGui::PopID();

					ImGui::TableSetColumnIndex(1);
					ImGui::PushID((int)element.key);
					drawCtx.changed |= DrawPrimitiveValue(element.field, (u8*)element.data, drawCtx.parentType, drawCtx.editorCtx);
					ImGui::PopID();
					return true;
				},
				parentType);

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
				const meTypeDescriptor* elemType = meTypeDescriptorGetSingleTemplateArg(*parentType);
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
	    changed = DrawAssetField(type, &asset, nullptr, ctx);
    }
    else if (&type == &TD_MAID)
    {
        MAID* maid = (MAID*)data;
        meAsset asset = *maid;
        changed = DrawAssetField(type, &asset, nullptr, ctx);
        *maid = asset.id;
    }
	else
	{
		return false; // not a primitive we handle
	}
	return changed;
}

static bool DrawTypeDescriptorFieldData(
	const meTypeDescriptor& field,
	u8* fieldData,
	AssetEditorContext* ctx,
	const meTypeDescriptor* parentType);

bool DrawStructFields(const meTypeDescriptor& type, u8* dataPtr, AssetEditorContext* ctx)
{
	bool anyChanged = false;
	meTypeDescriptorWalkMembers(type, dataPtr,
		[&](const meTypeDescriptorMember& member)
		{
			ImGui::PushID((int)member.index);
			if (DrawTypeDescriptorFieldData(member.field, (u8*)member.data, ctx, &type))
				anyChanged = true;
			ImGui::PopID();
			return true;
		});
	return anyChanged;
}


// dataPtr is the base address of the parent struct
bool DrawTypeDescriptorField(
	const meTypeDescriptor& field,
	u8* dataPtr,
	AssetEditorContext* ctx,
	const meTypeDescriptor* parentType)
{
	if (!dataPtr) return false;
	u8* fieldData = dataPtr + meTypeDescriptorMemberOffsetBytes(field);
	return DrawTypeDescriptorFieldData(field, fieldData, ctx, parentType);
}

static bool DrawTypeDescriptorFieldData(
	const meTypeDescriptor& field,
	u8* fieldData,
	AssetEditorContext* ctx,
	const meTypeDescriptor* parentType)
{
	const meTypeDescriptor* fieldType = field.thisType;
	bool changed = false;

	if (!fieldType) return false;

	const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();

    if (fieldType == &TD_MESERIALIZEDHEADER)
    {
        return false;
    }

    if (fieldType->editorRenderFn)
    {
        changed |= InspectorLabel(field, fieldData, parentType, ctx);
		ImGui::PushID(displayName);
		ImGui::BeginGroup();
        EditorRenderContext editorRenderCtx{fieldData};
        changed |= fieldType->editorRenderFn(editorRenderCtx);
		ImGui::EndGroup();
		ImGui::PopID();
    }
	else if (fieldType == &TD_MEASSET || fieldType->thisType == &TD_MEASSET)
	{
		meAsset& asset = *(meAsset*)fieldData;
		changed = DrawAssetField(field, &asset, parentType, ctx);
	}
	else if (fieldType == &TD_MAID)
	{
		MAID* maid = (MAID*)fieldData;
		meAsset asset = *maid;
		changed = DrawAssetField(field, &asset, parentType, ctx);
		*maid = asset.id;
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
		changed |= DrawTypeDescriptorFieldContextMenu("##field_context_label", field, fieldData, parentType, ctx);
		ImGui::TableSetColumnIndex(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(STRING_FMT, STRING_VAARGS(fieldType->name));
		ImGui::PopStyleColor();

		if (nodeOpen)
		{
			changed |= DrawStructFields(*fieldType, fieldData, ctx);
			ImGui::TreePop();
		}
	}
	else
	{
		changed |= InspectorLabel(field, fieldData, parentType, ctx);
		ImGui::PushID(displayName);
		changed |= DrawPrimitiveValue(*fieldType, fieldData, &field, ctx);
		ImGui::PopID();

		if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
		}
	}
	changed |= DrawTypeDescriptorFieldContextMenu("##field_context_value", field, fieldData, parentType, ctx);
	return changed;
}

