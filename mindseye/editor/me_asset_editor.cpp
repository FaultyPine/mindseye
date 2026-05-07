#include "me_asset_editor.h"

#define IMGUI_NODE_EDITOR_API MEAPI
#include "imgui_node_editor.h"
#include "external/bgfx/bgfx/3rdparty/dear-imgui/imgui.h"

#include "core/me_core.h"
#include "core/me_app.h"
#include "core/me_log.h"
#include "scene/me_entity.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"

namespace ed = ax::NodeEditor;

// ID encoding: bases occupy distinct high bits so ranges don't overlap.
// Asset node:   ASSET_NODE_BASE  + (type << 32) + id
// Entity pin:   PIN_BASE + ref + (fieldIdx << 32) + (1 << 40)
// Asset pin:    PIN_BASE + ASSET_NODE_BASE + (type << 32) + id
// Link:         LINK_BASE + ref + (fieldIdx << 32)

STATIC_ASSERT(sizeof(((MAID*)0)->id)   == sizeof(u32));
STATIC_ASSERT(sizeof(((MAID*)0)->type) == sizeof(u32));
STATIC_ASSERT(NUM_ASSET_TYPES < 256);

static constexpr u64 ASSET_NODE_BASE  = 0x2000000000ULL;
static constexpr u64 PIN_BASE         = 0x4000000000ULL;
static constexpr u64 LINK_BASE        = 0x8000000000ULL;

static ed::NodeId MakeAssetNodeId(MAID maid)
{
	return ed::NodeId(ASSET_NODE_BASE + ((u64)maid.type << 32) + (u64)maid.id);
}

static ed::PinId MakeAssetFieldOutputPin(meAsset ref, u32 fieldIdx)
{
	u64 v = PIN_BASE + (u64)ref.id + ((u64)fieldIdx << 32) + (1ULL << 40);
	return ed::PinId(v);
}

static ed::PinId MakeAssetInputPin(MAID maid)
{
	u64 v = PIN_BASE + ASSET_NODE_BASE + ((u64)maid.type << 32) + (u64)maid.id;
	return ed::PinId(v);
}

static ed::LinkId MakeLinkId(EntityRef ref, u32 fieldIdx)
{
	return ed::LinkId(LINK_BASE + (u64)ref + ((u64)fieldIdx << 32));
}

struct MAIDFieldInfo
{
	const meTypeDescriptor* field;
	MAID* maidPtr;
	u32 fieldIndex;
};

static void CollectMAIDFields(const meTypeDescriptor& typeDesc, u8* basePtr, 
                               MAIDFieldInfo* outFields, u32* outCount, u32 maxFields, u32 baseFieldIdx = 0)
{
	extern meTypeDescriptor TD_MAID;
	extern meTypeDescriptor TD_MEASSET;
	for (u32 i = 0; i < typeDesc.fields.size && *outCount < maxFields; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_PaddingMember)) continue;
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_Excluded)) continue;
		
		u8* fieldData = basePtr + (field.offsetBits / 8);
		const meTypeDescriptor* fieldType = field.thisType;
		if (!fieldType) continue;

		if (fieldType == &TD_MAID)
		{
			MAIDFieldInfo info = {};
			info.field = &field;
			info.maidPtr = (MAID*)fieldData;
			info.fieldIndex = baseFieldIdx + i;
			outFields[(*outCount)++] = info;
		}
		else if (fieldType == &TD_MEASSET)
		{
			MAIDFieldInfo info = {};
			info.field = &field;
			info.maidPtr = &((meAsset*)fieldData)->id;
			info.fieldIndex = baseFieldIdx + i;
			outFields[(*outCount)++] = info;
		}
		else if (fieldType->fields.size > 0)
		{
			CollectMAIDFields(*fieldType, fieldData, outFields, outCount, maxFields, baseFieldIdx + i * 100);
		}
	}
}

static void DrawAssetReference(MAID maid)
{
	StringView path = meAssetIndexGetFilesystemPath(maid);
	const char* typeName = meAssetTypeToString(maid.GetType()).cstr();
	
	ed::BeginNode(MakeAssetNodeId(maid));
	
	ed::BeginPin(MakeAssetInputPin(maid), ed::PinKind::Input);
	ImGui::Text(ICON_FA_CIRCLE_LEFT);
	ed::EndPin();
	
	ImGui::SameLine();
	
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.98f, 0.48f, 1.00f));
	ImGui::Text(ICON_FA_FILE " %s", typeName);
	ImGui::PopStyleColor();
	
	if (path.data && path.len)
	{
		ImGui::Text(STRING_FMT, STRING_VAARGS(path));
	}
	else
	{
		ImGui::TextDisabled("(no file)");
	}
	
	ed::EndNode();
}

static void DrawAssetPickerPopup(AssetEditorContext& ctx)
{
	AssetEditorPendingLink& pendingLink = ctx.pendingLink;
	if (!pendingLink.active) return;
	
	ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopup("AssetPicker"))
	{
		ImGui::Text("Select %s asset:", meAssetTypeToString(pendingLink.expectedType).cstr());
		ImGui::Separator();
		
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##search", "Search...", ctx.searchBuf, sizeof(ctx.searchBuf));
		
		ImGui::Separator();
		
		if (ImGui::Selectable("(none)"))
		{
			pendingLink.sourceMaidPtr->SetID(U32_INVALID_ID);
			pendingLink.active = false;
			ctx.searchBuf[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
		
		const meAssetIndex& index = meAssetIndexGetRO();
		for (auto& [id, path] : index.assetToPathMap)
		{
			if (id.GetType() != pendingLink.expectedType) continue;
			
			if (ctx.searchBuf[0] != '\0')
			{
				bool matched = false;
				const char* pathCstr = path.cstr();
				const char* found = pathCstr;
				while (*found)
				{
					const char* s = ctx.searchBuf;
					const char* p = found;
					bool ok = true;
					while (*s && *p)
					{
						char a = (*s >= 'A' && *s <= 'Z') ? *s + 32 : *s;
						char b = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
						if (a != b) { ok = false; break; }
						s++; p++;
					}
					if (ok && *s == '\0') { matched = true; break; }
					found++;
				}
				if (!matched) continue;
			}
			
			if (ImGui::Selectable(path.cstr(), *pendingLink.sourceMaidPtr == id))
			{
				*pendingLink.sourceMaidPtr = id;
				pendingLink.active = false;
				ctx.searchBuf[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
		}
		
		ImGui::EndPopup();
	}
	else
	{
		pendingLink.active = false;
		ctx.searchBuf[0] = '\0';
	}
}

static void DrawAssetNode(meAsset asset, meSpanTyped<MAIDFieldInfo> maidFields)
{
    meAssetType assetType = asset.id.GetType();
    if (assetType == MABadData || assetType >= NUM_ASSET_TYPES) return;

    meAssetSystem& sys = meAssetSystemGet();
    meAssetLoader* loader = sys.assetLoaders[assetType];
    if (!loader || !loader->assetTypeDesc || !loader->resourcePool) return;

    void* dataOpaque = loader->resourcePool->GetOpaque(asset.runtimeHandle);
    if (!dataOpaque) return;

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

    ImGui::Separator();

    if (ImGui::BeginTable("##fields", 2,
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawStructFields(typeDesc, dataPtr);
        ImGui::EndTable();
    }

    for (u32 i = 0; i < maidFields.size; i++)
    {
        const char* fieldName = maidFields[i].field->editorName.data
            ? maidFields[i].field->editorName.cstr()
            : maidFields[i].field->name.cstr();
        ed::BeginPin(MakeAssetFieldOutputPin(asset, maidFields[i].fieldIndex), ed::PinKind::Output);
        ImGui::Text(ICON_FA_CIRCLE_RIGHT " %s", fieldName);
        ed::EndPin();
    }

    ed::EndNode();
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
	ctx.isOpen = true;
	ctx.needsNavigateToContent = true;
}

void meAssetEditorTick(AssetEditorContext& ctx)
{
	if (!ctx.isOpen) return;
	
	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(ICON_FA_DIAGRAM_PROJECT " Asset Editor", &ctx.isOpen))
	{
		ImGui::End();
		return;
	}
	
    meAsset& asset = ctx.rootAsset;
	if (!asset.isValid())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("No entity selected.");
		ImGui::PopStyleColor();
		ImGui::End();
		return;
	}
		
	ed::SetCurrentEditor(ctx.nodeEditorCtx);
	ed::Begin("AssetEditor");
	
    meAssetSystem& sys = meAssetSystemGet();
    meAssetLoader* loader = sys.assetLoaders[asset.id.GetType()];
    if (!loader || !loader->assetTypeDesc || !loader->resourcePool) return;

    void* dataOpaque = loader->resourcePool->GetOpaque(asset.runtimeHandle);
    if (!dataOpaque) return;
    u8* dataPtr = (u8*)dataOpaque;

	static constexpr u32 MAX_MAID_FIELDS = 32;
	MAIDFieldInfo maidFields[MAX_MAID_FIELDS] = {};
	u32 maidFieldCount = 0;
	
	CollectMAIDFields(TD_MEENTITY, dataPtr, maidFields, &maidFieldCount, MAX_MAID_FIELDS);
	
	DrawAssetNode(asset, {maidFields, maidFieldCount});

	for (u32 i = 0; i < maidFieldCount; i++)
	{
		MAIDFieldInfo& info = maidFields[i];
		MAID maid = *info.maidPtr;
		
		if (!maid) continue;
		
		DrawAssetReference(maid);
		
		ed::Link(
			MakeLinkId(asset, info.fieldIndex),
			MakeAssetFieldOutputPin(asset, info.fieldIndex),
			MakeAssetInputPin(maid),
			ImVec4(0.74f, 0.58f, 0.98f, 1.00f),
			2.0f
		);
	}
	
	// Dragging from an output pin into empty space opens the asset picker
	if (ed::BeginCreate())
	{
		ed::PinId startPinId, endPinId;
		if (ed::QueryNewNode(&startPinId))
		{
			if (ed::AcceptNewItem())
			{
				for (u32 i = 0; i < maidFieldCount; i++)
				{
					ed::PinId expectedPin = MakeAssetFieldOutputPin(asset, maidFields[i].fieldIndex);
					if (expectedPin == startPinId)
					{
						ctx.pendingLink.active = true;
						ctx.pendingLink.sourcePinId = startPinId.Get();
						ctx.pendingLink.sourceAsset = asset;
						ctx.pendingLink.sourceFieldIndex = maidFields[i].fieldIndex;
						ctx.pendingLink.sourceMaidPtr = maidFields[i].maidPtr;
						ctx.pendingLink.expectedType = maidFields[i].maidPtr->GetType();
						
						ed::Suspend();
						ImGui::OpenPopup("AssetPicker");
						ed::Resume();
						break;
					}
				}
			}
		}
	}
	ed::EndCreate();
	
	if (ed::BeginDelete())
	{
		ed::LinkId deletedLinkId;
		while (ed::QueryDeletedLink(&deletedLinkId))
		{
			if (ed::AcceptDeletedItem())
			{
				for (u32 i = 0; i < maidFieldCount; i++)
				{
					ed::LinkId expectedLink = MakeLinkId(asset, maidFields[i].fieldIndex);
					if (expectedLink == deletedLinkId)
					{
						maidFields[i].maidPtr->SetID(U32_INVALID_ID);
						break;
					}
				}
			}
		}
	}
	ed::EndDelete();
	
	// Must be drawn inside ed context but suspended (ImGui popups need screen-space coords)
	if (ctx.pendingLink.active)
	{
		ed::Suspend();
		DrawAssetPickerPopup(ctx);
		ed::Resume();
	}
	
	if (ctx.needsNavigateToContent)
	{
		ed::NavigateToContent();
		ctx.needsNavigateToContent = false;
	}
	
	ed::End();
	ed::SetCurrentEditor(nullptr);

    ctx.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ImGui::End();
}







// Generic editor type drawing


// Label helper: draw a left-aligned label in a two-column table row.
// The caller must be inside a BeginTable() with at least 2 columns.
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
    MAID* maid)
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

    if ((u32)assetType == U32_INVALID_ID)
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

    if (ImGui::BeginCombo("##v", preview))
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
        ImGui::EndCombo();
    }
    return changed;
}

bool DrawPrimitiveValue(const meTypeDescriptor& type, u8* data, const meTypeDescriptor* parentType)
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
		const char* preview = (str->data && str->len) ? str->cstr() : "";
        ImGui::PushID(preview);
        bool textChangedAtAll = ImGui::InputTextWithHint("##MyInput", "Enter text here...", s_textEditIntermediateBuf, IM_ARRAYSIZE(s_textEditIntermediateBuf));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            changed |= textChangedAtAll;
        }
        ImGui::PopID();
	}
	else if (&type == &TD_STRINGVIEW)
	{
		StringView* sv = (StringView*)data;
        ImGui::PushID(sv->cstr());
		bool textChangedAtAll = ImGui::InputTextWithHint("##MyInput", "Enter text here...", s_textEditIntermediateBuf, IM_ARRAYSIZE(s_textEditIntermediateBuf));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            changed |= textChangedAtAll;
        }
        ImGui::PopID();
	}
	else if (&type == &TD_DYNARRAY)
	{
		bool changed = false;
		// Assume DynArray<T> layout: struct { T* data; u32 size; u32 capacity; }
		struct DynArrayHeader { void* data; u32 size; u32 capacity; };
		DynArrayAny& arr = *(DynArrayAny*)data;
        ME_ASSERT(arr); // we expect the asset loader to initialize these sorts of internal things
		if (!parentType->templatedTypes)
			return false;
		const meTypeDescriptor& elemType = *parentType->templatedTypes[0];

		if (ImGui::BeginTable("##dynarray_table", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 32.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			for (u32 i = 0; i < DynArrayGetSize(arr); ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%u", i);
				ImGui::SameLine();
				// Remove button
				ImGui::PushID(i);
				if (ImGui::SmallButton("-"))
				{
					// Shift elements down
					DynArrayPopAt(arr, i);
					changed = true;
					ImGui::PopID();
					break; // Only one change per frame
				}
				ImGui::PopID();
				ImGui::TableSetColumnIndex(1);
				u8* elemPtr = (u8*)arr.data + (i * elemType.size);
				ImGui::PushID((int)i);
				changed |= DrawPrimitiveValue(elemType, elemPtr, parentType);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		// Add button
		if (ImGui::Button("Add"))
		{
            // blank allocation with defaults, since it'll get copied into the correct asset memory anyway
            Allocation elementData = MECALLOC(GetTLScratch(), elemType.size);
            elemType.setToDefaultsFn(elementData);
            DynArrayPush(arr, (u8*)elementData, elementData.size);
			changed = true;
		}
		return changed;
	}
    else if (&type == &TD_MEASSET || type.thisType == &TD_MEASSET)
    {
        // Matches both TD_MEASSET directly and intermediate typed-asset descriptors
        // (e.g. g_typearg_entities_0 for DynArray<meTypedAsset<MAEntity>> elements),
        // which have .thisType == &TD_MEASSET and carry the enum value in templatedTypes[0].
        meAsset& asset = *(meAsset*)data;
        MAID& maid = asset.id;
	    changed = DrawAssetField(type, &maid);
    }
    else if (&type == &TD_MAID)
    {
        MAID* maid = (MAID*)data;
        changed = DrawAssetField(type, maid);
    }
	else
	{
		return false; // not a primitive we handle
	}
	return changed;
}

bool DrawStructFields(const meTypeDescriptor& type, u8* dataPtr)
{
	bool anyChanged = false;
	for (u32 i = 0; i < type.fields.size; i++)
	{
		const meTypeDescriptor& field = type.fields[i];
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_PaddingMember)) continue;
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_Excluded)) continue;

		ImGui::PushID((int)i);
		if (DrawTypeDescriptorField(field, dataPtr))
			anyChanged = true;
		ImGui::PopID();
	}
	return anyChanged;
}


// dataPtr is the base address of the parent struct
bool DrawTypeDescriptorField(const meTypeDescriptor& field, u8* dataPtr)
{
	u8* fieldData = dataPtr + (field.offsetBits / 8);
	const meTypeDescriptor* fieldType = field.thisType;
	bool changed = false;

	if (!fieldType) return false;

	const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();

	// it's a struct -> show as tree node
	if (fieldType->fields.size > 0)
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
			changed = DrawStructFields(*fieldType, fieldData);
			ImGui::TreePop();
		}
	}
	else
	{
		InspectorLabel(displayName);
		ImGui::PushID(displayName);
		changed = DrawPrimitiveValue(*fieldType, fieldData, &field);
		ImGui::PopID();

		if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
		}
	}
	return changed;
}

