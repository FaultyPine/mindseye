#include "me_editor.h"

#include "external/bgfx/bgfx/3rdparty/dear-imgui/imgui.h"

#ifdef COMPILER_CLANG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#endif
#include "res/IconsFontAwesome6.h"
#include "ImGuiNotify.hpp"
#include "res/fa-solid-900.h"
#ifdef COMPILER_CLANG
#pragma GCC diagnostic pop
#endif

#include "external/potable-file-dialogs.h"

#include "core/me_event.h"
#include "core/me_command.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"
#include "scene/me_scene.h"

EditorContext& meEditorGetCtx()
{
	return *GetEngineCtx()->editor;
}

void meEditorOnAssetBeginLoading(meEventPayload payload)
{
	//const MAID& id = *(MAID*)payload.payload;
	//ImGui::InsertNotification({ImGuiToastType::Info, 3000, "Began loading " STRING_FMT, STRING_VAARGS(meAssetIndexGetFilesystemPath(id))});
}

void meEditorOnAssetFinishedLoading(meEventPayload payload)
{
	const MAID& id = *(MAID*)payload.payload;
	StringView diskPath = meAssetIndexGetFilesystemPath(id);
	ImGui::InsertNotification({ImGuiToastType::Info, 3000, StringFormatTmp("Finished loading " STRING_FMT, STRING_VAARGS(diskPath)).cstr()});
}

static void SetupImGuiDraculaStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// --- 1. Sizing and Spacing (Clean & Balanced) ---
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(6.0f, 4.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.ScrollbarSize = 14.0f;
	style.GrabMinSize = 12.0f;

	// --- 2. Borders & Rounding ---
	style.WindowRounding = 6.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 12.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;

	// --- 3. The Dracula Color Palette ---
	colors[ImGuiCol_Text] = ImVec4(0.97f, 0.97f, 0.95f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);

	colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.21f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.16f, 0.21f, 0.96f);

	colors[ImGuiCol_Border] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

	colors[ImGuiCol_FrameBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

	colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);

	colors[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);

	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

	colors[ImGuiCol_CheckMark] = ImVec4(0.31f, 0.98f, 0.48f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.84f, 0.68f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.47f, 0.78f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.37f, 0.62f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

	colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);

	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);

	colors[ImGuiCol_PlotLines] = ImVec4(0.55f, 0.91f, 0.99f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
	colors[ImGuiCol_NavCursor] = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);

#ifdef IMGUI_HAS_DOCK
	colors[ImGuiCol_DockingPreview] = ImVec4(0.74f, 0.58f, 0.98f, 0.50f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
#endif
}

void meEditorInitialize(EngineContext* engine)
{
	engine->editor = MENEW(&engine->engineArena, EditorContext);

	meEventSubscribe(engine->assetSystem->assetBeginLoadingEvent, meEditorOnAssetBeginLoading);
	meEventSubscribe(engine->assetSystem->assetFinishedLoadingEvent, meEditorOnAssetFinishedLoading);

	SetupImGuiDraculaStyle();

	ImGui::GetIO().Fonts->AddFontDefault();

	float baseFontSize = 16.0f;
	float iconFontSize = baseFontSize * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

	static constexpr ImWchar iconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
	ImFontConfig iconsConfig;
	iconsConfig.MergeMode = true;
	iconsConfig.PixelSnapH = true;
	iconsConfig.GlyphMinAdvanceX = iconFontSize;
	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, iconFontSize, &iconsConfig, iconsRanges);

	meAssetEditorInitialize(engine->editor->assetEditor);
}


static void DrawEntityInspector(EngineContext* engine);

void meEditorTick(EngineContext* engine)
{
	EditorContext& editor = meEditorGetCtx();

	// Entity picking on left click (only when cursor is free / not captured by camera)
	if (engine->osData->cursorState == FREE &&
		engine->osData->mouseState.IsMouseButtonJustPressed(LBUTTON) &&
		!ImGui::GetIO().WantCaptureMouse)
	{
		meExternalCommand cmd = {};
		cmd.type = meExternalCommandType_PickEntity;
		cmd.pickEntity.screenPos = engine->osData->mouseState.mousePosScreen;
		meReceiveExternalCommand(cmd);
	}

    // TODO: debug draw main scene camera
	// engine->sceneSystem->CurrentScene().mainCamera.cameraPos

	DrawEntityInspector(engine);

	meAssetEditorTick(editor.assetEditor);

	// Camera updates happen after editor windows so input blocking is set in time
    GetEngineCtx()->osData->userInputBlocked = editor.assetEditor.isFocused;
	editor.editorCamera.UpdateCameraWithUserInput(*engine->osData);

	// Notifications style setup
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f); // Disable round borders
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f); // Disable borders
	// Notifications color setup
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f)); // Background color
	// Main rendering function
	ImGui::RenderNotifications();
	// Argument MUST match the amount of ImGui::PushStyleVar() calls 
	ImGui::PopStyleVar(2);
	// Argument MUST match the amount of ImGui::PushStyleColor() calls 
	ImGui::PopStyleColor(1);
   
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("New"))
			{
				auto openFileResult = pfd::save_file("Creating new scene file", ".").result();
                if (!openFileResult.empty())
                {
					const char* fileCstr = openFileResult.c_str();
					StringView openFilename = StringFromCString(fileCstr);
					meExternalCommand cmd = {};
					cmd.type = meExternalCommandType_CreateNewScene;
					cmd.createNewScene.path = openFilename;
					meReceiveExternalCommand(cmd);
                }
			}
            if (ImGui::MenuItem("Open"))
            {
                auto openFileResult = pfd::open_file("Select a scene file", ".").result();
                if (!openFileResult.empty())
                {
                    ME_ASSERT(openFileResult.size() == 1);
					const char* fileCstr = openFileResult[0].c_str();
                    StringView sceneFile = StringFromCString(fileCstr);
					meExternalCommand cmd = {};
					cmd.type = meExternalCommandType_ChangeScene;
					cmd.changeScene.path = sceneFile;
					meReceiveExternalCommand(cmd);
                }
            }
            if (ImGui::MenuItem("Save Current"))
            {
				meExternalCommand cmd = {};
				cmd.type = meExternalCommandType_SaveCurrentScene;
				// if the scene has no disk path yet, ask the user for one
				// kept in outer scope so the string data outlives the command dispatch
				std::vector<std::string> openFileResult;
				meScene& currentScene = engine->sceneSystem->CurrentScene();
                if (!meAssetIndexGetFilesystemPath(currentScene.header))
                {
                    openFileResult = pfd::open_file("Location to save the scene file", ".").result();
                    if (!openFileResult.empty())
                    {
                        ME_ASSERT(openFileResult.size() == 1);
                        const char* fileCstr = openFileResult[0].c_str();
                        cmd.saveCurrentScene.path = StringFromCString(fileCstr);
                    }
                }
				meReceiveExternalCommand(cmd);
            }
			if (ImGui::BeginMenu("Entity"))
			{
				if (ImGui::MenuItem("New"))
				{
					meExternalCommand cmd = {};
					cmd.type = meExternalCommandType_CreateEntity;
					meReceiveExternalCommand(cmd);
				}
				ImGui::EndMenu();
			}
            ImGui::EndMenu();
        }

		
		StringView rightAlignedText = StringFormatTmp("Avg framerate: %6.2f | %.*s", ImGui::GetIO().Framerate, STRING_VAARGS(engine->appConfig.appName));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(rightAlignedText.cstr()).x
							 - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
		ImGui::TextEx(rightAlignedText.cstr());

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar();

}










// ============================================================================
// Entity Inspector
// ============================================================================

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

static bool DrawTypeDescriptorField(const meTypeDescriptor& field, u8* dataPtr);

static bool DrawPrimitiveValue(const meTypeDescriptor& type, u8* data)
{
	bool changed = false;

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
		ImGui::TextUnformatted(preview);
	}
	else if (&type == &TD_STRINGVIEW)
	{
		StringView* sv = (StringView*)data;
		ImGui::Text(STRING_FMT, STRING_VAARGS((*sv)));
	}
	else
	{
		return false; // not a primitive we handle
	}
	return changed;
}

static bool DrawStructFields(const meTypeDescriptor& type, u8* dataPtr)
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
static bool DrawTypeDescriptorField(const meTypeDescriptor& field, u8* dataPtr)
{
	u8* fieldData = dataPtr + (field.offsetBits / 8);
	const meTypeDescriptor* fieldType = field.thisType;
	bool changed = false;

	if (!fieldType) return false;

	const char* displayName = field.editorName.data ? field.editorName.cstr() : field.name.cstr();

	// Special case: MAID or meAsset asset reference - show an asset browser combo
	MAID* maid = nullptr;
	if (fieldType == &TD_MAID)
	{
		maid = (MAID*)fieldData;
	}
	else if (fieldType == &TD_MEASSET)
	{
		maid = &((meAsset*)fieldData)->id;
	}
	if (maid)
	{
		meAssetType assetType = maid->GetType();

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
				if (id.GetType() != assetType) continue;

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
		changed = DrawPrimitiveValue(*fieldType, fieldData);
		ImGui::PopID();

		if (field.tooltip.data && field.tooltip.len > 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(STRING_FMT, STRING_VAARGS(field.tooltip));
		}
	}
	return changed;
}

static void DrawEntityInspector(EngineContext* engine)
{
	EditorContext& editor = meEditorGetCtx();

	ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector"))
	{
		ImGui::End();
		return;
	}

	if (!editor.selectedEntity)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("No entity selected. Click an entity in the viewport to inspect it.");
		ImGui::PopStyleColor();
		ImGui::End();
		return;
	}

	EntityData& entity = Entity::GetEntity(editor.selectedEntity);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.18f, 1.00f));
	ImGui::BeginChild("##inspector_header", ImVec2(0, 56), ImGuiChildFlags_Borders);
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.74f, 0.58f, 0.98f, 1.00f));
		ImGui::Text(ICON_FA_CUBE);
		ImGui::PopStyleColor();
		ImGui::SameLine();

		char nameBuf[256] = {};
		if (entity.name.data && entity.name.len > 0)
		{
			u64 copyLen = entity.name.len < sizeof(nameBuf) - 1 ? entity.name.len : sizeof(nameBuf) - 1;
			memcpy(nameBuf, entity.name.data, copyLen);
		}
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputText("##entity_name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_ReadOnly);

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text("ID: %u", editor.selectedEntity.ref);
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Spacing();

	if (ImGui::Button(ICON_FA_DIAGRAM_PROJECT " Open in Asset Editor"))
	{
		meAssetEditorOpen(editor.assetEditor, editor.selectedEntity);
	}

	ImGui::Spacing();

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_BordersInnerH |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_PadOuterX;

	if (ImGui::BeginTable("##inspector_props", 2, tableFlags))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);

		DrawStructFields(TD_ENTITYDATA, (u8*)&entity);

		ImGui::EndTable();
	}

	ImGui::End();
}