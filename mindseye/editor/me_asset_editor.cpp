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
// Entity node:  ENTITY_NODE_BASE + ref
// Asset node:   ASSET_NODE_BASE  + (type << 32) + id
// Entity pin:   PIN_BASE + ref + (fieldIdx << 32) + (1 << 40)
// Asset pin:    PIN_BASE + ASSET_NODE_BASE + (type << 32) + id
// Link:         LINK_BASE + ref + (fieldIdx << 32)

STATIC_ASSERT(sizeof(((MAID*)0)->id)   == sizeof(u32));
STATIC_ASSERT(sizeof(((MAID*)0)->type) == sizeof(u32));
STATIC_ASSERT(NUM_ASSET_TYPES < 256);
STATIC_ASSERT(sizeof(((EntityRef*)0)->ref) == sizeof(u32));

static constexpr u64 ENTITY_NODE_BASE = 0x1000000000ULL;
static constexpr u64 ASSET_NODE_BASE  = 0x2000000000ULL;
static constexpr u64 PIN_BASE         = 0x4000000000ULL;
static constexpr u64 LINK_BASE        = 0x8000000000ULL;

static ed::NodeId MakeEntityNodeId(EntityRef ref)
{
	return ed::NodeId(ENTITY_NODE_BASE + (u64)ref.ref);
}

static ed::NodeId MakeAssetNodeId(MAID maid)
{
	return ed::NodeId(ASSET_NODE_BASE + ((u64)maid.type << 32) + (u64)maid.id);
}

static ed::PinId MakeEntityFieldOutputPin(EntityRef ref, u32 fieldIdx)
{
	u64 v = PIN_BASE + (u64)ref.ref + ((u64)fieldIdx << 32) + (1ULL << 40);
	return ed::PinId(v);
}

static ed::PinId MakeAssetInputPin(MAID maid)
{
	u64 v = PIN_BASE + ASSET_NODE_BASE + ((u64)maid.type << 32) + (u64)maid.id;
	return ed::PinId(v);
}

static ed::LinkId MakeLinkId(EntityRef ref, u32 fieldIdx)
{
	return ed::LinkId(LINK_BASE + (u64)ref.ref + ((u64)fieldIdx << 32));
}

static bool DrawNodePrimitiveValue(const meTypeDescriptor& type, u8* data)
{
	bool changed = false;

	extern meTypeDescriptor TD_FLOAT;
	extern meTypeDescriptor TD_DOUBLE;
	extern meTypeDescriptor TD_INT;
	extern meTypeDescriptor TD_UNSIGNED_INT;
	extern meTypeDescriptor TD_BOOL;
	extern meTypeDescriptor TD_VEC3;
	extern meTypeDescriptor TD_QUAT;
	extern meTypeDescriptor TD_STRING;
	extern meTypeDescriptor TD_STRINGVIEW;

	if (&type == &TD_FLOAT)
	{
		ImGui::SetNextItemWidth(120);
		changed = ImGui::DragFloat("##v", (float*)data, 0.01f);
	}
	else if (&type == &TD_DOUBLE)
	{
		float tmp = (float)*(double*)data;
		ImGui::SetNextItemWidth(120);
		if (ImGui::DragFloat("##v", &tmp, 0.01f)) { *(double*)data = tmp; changed = true; }
	}
	else if (&type == &TD_INT)
	{
		ImGui::SetNextItemWidth(120);
		changed = ImGui::DragInt("##v", (int*)data);
	}
	else if (&type == &TD_UNSIGNED_INT)
	{
		ImGui::SetNextItemWidth(120);
		changed = ImGui::DragScalar("##v", ImGuiDataType_U32, data);
	}
	else if (&type == &TD_BOOL)
	{
		changed = ImGui::Checkbox("##v", (bool*)data);
	}
	else if (&type == &TD_VEC3)
	{
		ImGui::SetNextItemWidth(200);
		changed = ImGui::DragFloat3("##v", (float*)data, 0.01f);
	}
	else if (&type == &TD_QUAT)
	{
		glm::quat& q = *(glm::quat*)data;
		glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
		ImGui::SetNextItemWidth(200);
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
		ImGui::TextUnformatted(preview);
	}
	else if (&type == &TD_STRINGVIEW)
	{
		StringView* sv = (StringView*)data;
		ImGui::Text(STRING_FMT, STRING_VAARGS((*sv)));
	}
	else
	{
		ImGui::TextDisabled("(unsupported)");
	}
	return changed;
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
		else if (fieldType->fields.size > 0)
		{
			CollectMAIDFields(*fieldType, fieldData, outFields, outCount, maxFields, baseFieldIdx + i * 100);
		}
	}
}

static void DrawEntityNode(EntityRef ref, EntityData& entity, 
                            MAIDFieldInfo* maidFields, u32 maidFieldCount)
{
	extern meTypeDescriptor TD_ENTITYDATA;
	extern meTypeDescriptor TD_MAID;
	
	ed::BeginNode(MakeEntityNodeId(ref));
	
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.74f, 0.58f, 0.98f, 1.00f));
	ImGui::Text(ICON_FA_CUBE " %s", 
		(entity.name.data && entity.name.len) ? entity.name.cstr() : "Entity");
	ImGui::PopStyleColor();
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::Text("ID: %u", ref.ref);
	ImGui::PopStyleColor();
	
	ImGui::Separator();
	
	u8* basePtr = (u8*)&entity;
	for (u32 i = 0; i < TD_ENTITYDATA.fields.size; i++)
	{
		const meTypeDescriptor& field = TD_ENTITYDATA.fields[i];
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_PaddingMember)) continue;
		if (TEST_BIT(field.flags, meTypeDescriptorFlag_Excluded)) continue;
		
		u8* fieldData = basePtr + (field.offsetBits / 8);
		const meTypeDescriptor* fieldType = field.thisType;
		if (!fieldType) continue;
		
		if (fieldType == &TD_MAID) continue; // MAID fields become output pins
		
		const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();
		
		if (fieldType->fields.size > 0)
		{
			ImGui::PushID((int)i);
			if (ImGui::TreeNodeEx(displayName, ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (u32 j = 0; j < fieldType->fields.size; j++)
				{
					const meTypeDescriptor& subField = fieldType->fields[j];
					if (TEST_BIT(subField.flags, meTypeDescriptorFlag_PaddingMember)) continue;
					if (TEST_BIT(subField.flags, meTypeDescriptorFlag_Excluded)) continue;
					if (!subField.thisType) continue;
					
					const char* subName = subField.editorName.data ? subField.editorName.cstr() : subField.name.cstr();
					u8* subData = fieldData + (subField.offsetBits / 8);
					
					ImGui::PushID((int)j);
					ImGui::Text("%s:", subName);
					ImGui::SameLine();
					DrawNodePrimitiveValue(*subField.thisType, subData);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		else
		{
			ImGui::PushID((int)i);
			ImGui::Text("%s:", displayName);
			ImGui::SameLine();
			DrawNodePrimitiveValue(*fieldType, fieldData);
			ImGui::PopID();
		}
	}
	
	if (maidFieldCount > 0)
	{
		ImGui::Separator();
		for (u32 i = 0; i < maidFieldCount; i++)
		{
			const MAIDFieldInfo& info = maidFields[i];
			const char* displayName = info.field->editorName.data 
				? info.field->editorName.cstr() 
				: info.field->name.cstr();
			
			ed::BeginPin(MakeEntityFieldOutputPin(ref, info.fieldIndex), ed::PinKind::Output);
			StringView currentPath = meAssetIndexGetFilesystemPath(*info.maidPtr);
			if (currentPath.data && currentPath.len)
			{
				ImGui::Text("%s " ICON_FA_CIRCLE_RIGHT, displayName);
			}
			else
			{
				ImGui::TextDisabled("%s " ICON_FA_CIRCLE_RIGHT, displayName);
			}
			ed::EndPin();
		}
	}
	
	ed::EndNode();
}

static void DrawAssetNode(MAID maid)
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

void meAssetEditorOpen(AssetEditorContext& ctx, EntityRef entity)
{
	ctx.rootEntity = entity;
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
	
	if (!ctx.rootEntity)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("No entity selected.");
		ImGui::PopStyleColor();
		ImGui::End();
		return;
	}
	
	EntityData& entity = Entity::GetEntity(ctx.rootEntity);
	
	ed::SetCurrentEditor(ctx.nodeEditorCtx);
	ed::Begin("AssetEditor");
	
	static constexpr u32 MAX_MAID_FIELDS = 32;
	MAIDFieldInfo maidFields[MAX_MAID_FIELDS] = {};
	u32 maidFieldCount = 0;
	
	extern meTypeDescriptor TD_ENTITYDATA;
	CollectMAIDFields(TD_ENTITYDATA, (u8*)&entity, maidFields, &maidFieldCount, MAX_MAID_FIELDS);
	
	DrawEntityNode(ctx.rootEntity, entity, maidFields, maidFieldCount);
	
	for (u32 i = 0; i < maidFieldCount; i++)
	{
		MAIDFieldInfo& info = maidFields[i];
		MAID maid = *info.maidPtr;
		
		if (!maid) continue;
		
		DrawAssetNode(maid);
		
		ed::Link(
			MakeLinkId(ctx.rootEntity, info.fieldIndex),
			MakeEntityFieldOutputPin(ctx.rootEntity, info.fieldIndex),
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
					ed::PinId expectedPin = MakeEntityFieldOutputPin(ctx.rootEntity, maidFields[i].fieldIndex);
					if (expectedPin == startPinId)
					{
						ctx.pendingLink.active = true;
						ctx.pendingLink.sourcePinId = startPinId.Get();
						ctx.pendingLink.sourceEntity = ctx.rootEntity;
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
					ed::LinkId expectedLink = MakeLinkId(ctx.rootEntity, maidFields[i].fieldIndex);
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
