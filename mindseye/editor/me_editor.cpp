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

#include "core/me_event.h"
#include "core/me_command.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"
#include "scene/me_scene.h"
#include "core/me_scope_exit.h"
#include "core/me_filesystem.h"

EditorContext& meEditorGetCtx()
{
	return *GetEngineCtx()->editor;
}

void meEditorOnAssetFinishedLoading(meEventPayload payload)
{
	const MAID& id = *(MAID*)payload.payload;
	StringView diskPath = meAssetIndexGetFilesystemPath(id);
	ImGui::InsertNotification({ImGuiToastType::Info, 3000, StringFormatTmp("Finished loading " STRING_FMT, STRING_VAARGS(diskPath)).cstr()});
}
void meEditorOnAssetFinishedWriting(meEventPayload payload)
{
	const MAID& id = *(MAID*)payload.payload;
	StringView diskPath = meAssetIndexGetFilesystemPath(id);
	ImGui::InsertNotification({ImGuiToastType::Info, 3000, StringFormatTmp("Finished writing " STRING_FMT, STRING_VAARGS(diskPath)).cstr()});
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

	meEventSubscribe(engine->assetSystem->assetFinishedLoadingEvent, meEditorOnAssetFinishedLoading);
	meEventSubscribe(engine->assetSystem->assetFinishedWritingEvent, meEditorOnAssetFinishedWriting);

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


static void PopulatePathFromUserInputIfNotValid(String& path)
{
    // if the scene has no disk path yet, ask the user for one
    // kept in outer scope so the string data outlives the command dispatch
    FilesystemPathPicker("Location to save the file", path);
}


static void DrawAssetInspector(EditorContext& editor, InspectorWindow& inspector)
{
    meAsset& asset = inspector.currentAsset;
    StringView title = StringFormatTmp(
        ICON_FA_CIRCLE_INFO " Inspector###Inspector_%u_%u",
        asset.id.GetType(),
        (u32)asset.id.GetID());

	ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title.cstr(), &inspector.active))
	{
        ImGui::End();
		return;
	}

    if (!asset.isLoaded() && asset.isValid())
    {
        // if it's not loaded, assume we want to inspect the template asset
        // if it's an instance, it must already be loaded by definition
        meAssetRequestLoadTemplate(&asset.id, 1);
        ImGui::Text("Loading asset...");
        ImGui::End();
        return;
    }

    StringView assetName = meAssetIndexGetFilesystemPath(asset);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.18f, 1.00f));
	ImGui::BeginChild("##inspector_header", ImVec2(0, 56), ImGuiChildFlags_Borders);
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.74f, 0.58f, 0.98f, 1.00f));
		ImGui::Text(ICON_FA_CUBE);
		ImGui::PopStyleColor();
		ImGui::SameLine();

        if (assetName)
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##asset_name", (char*)assetName.cstr(), assetName.len, ImGuiInputTextFlags_ReadOnly);
        }
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Spacing();

	if (asset.IsTemplateAsset() && ImGui::Button(ICON_FA_DIAGRAM_PROJECT " Open in Asset Editor"))
	{
		meAssetEditorOpen(editor.assetEditor, editor.editorSelectedObj.id ? editor.editorSelectedObj : inspector.currentAsset);
	}

	ImGui::Spacing();

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_BordersInnerH |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingFixedFit |
		ImGuiTableFlags_PadOuterX;

	if (ImGui::BeginTable("##inspector_props", 2, tableFlags))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        meTypeDescriptor* assetTypeDesc = meAssetSystemGet().assetLoaders[asset.id.GetType()]->assetTypeDesc;
        void* assetData = meAssetSystemGet().assetLoaders[asset.id.GetType()]->resourcePool->GetOpaque(asset.runtimeHandle);
		bool didUserChangeSomething = DrawStructFields(*assetTypeDesc, (u8*)assetData);
        if (didUserChangeSomething)
        {
            // Not supporting template asset editing in the inspector - it's only for runtime stuff 
            if (assetName)
            {
                LOG_INFO("User edited runtime object " STRING_FMT, STRING_VAARGS(assetName));
            }
        }

		ImGui::EndTable();
	}

	ImGui::End();
}



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
		meSendExternalCommand(cmd);
	}

    // TODO: debug draw main scene camera
	// engine->sceneSystem->CurrentScene().mainCamera.cameraPos
    for (u32 i = 0; i < editor.inspectors.size(); i++)
    {
        if (!editor.inspectors[i].active) continue;
        DrawAssetInspector(editor, editor.inspectors[i]);
    }

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
		if (ImGui::BeginMenu("Asset"))
		{
			if (ImGui::BeginMenu("New"))
			{
                meExternalCommand cmd = {};
                cmd.type = meExternalCommandType_CreateAsset;
                cmd.createAsset.type = MABadData;
                for (u32 i = 0; i < NUM_ASSET_TYPES; i++)
                {
                    StringView assetTypeStr = meAssetTypeToString((meAssetType)i);
                    if (ImGui::MenuItem(assetTypeStr.cstr()))
                    {
                        cmd.createAsset.type = (meAssetType)i;
                        break;
                    }
                }
                if (cmd.createAsset.type != MABadData)
                {
                    PopulatePathFromUserInputIfNotValid(cmd.createAsset.path);
                    meSendExternalCommand(cmd);
                }
                ImGui::EndMenu();
			}
            if (ImGui::BeginMenu("Open"))
            {
                meExternalCommand cmd = {};
                for (u32 i = 0; i < NUM_ASSET_TYPES; i++)
                {
                    meAssetType assetType = (meAssetType)i;
                    StringView assetTypeStr = meAssetTypeToString(assetType);
                    if (ImGui::BeginMenu(assetTypeStr.cstr()))
                    {
                        // simple stupid thing for now, in the future: a typical asset browser or maybe something else
                        for (const auto& [path, assetID] : meAssetIndexGetRO().pathToAssetsMap)
                        {
                            if (assetID.GetType() != assetType) continue;
                            if (ImGui::MenuItem(path.cstr()))
                            {
                                MAID maid = assetID;
                                meAssetRequestLoadTemplate(&maid, 1);
                                if (meAsset* tmplAsset = meAssetTryGetTemplate(maid))
                                {
                                    meAssetEditorOpen(editor.assetEditor, *tmplAsset);
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Change Scene"))
        {
            meExternalCommand cmd = {};
            cmd.type = meExternalCommandType_ChangeScene;
            PopulatePathFromUserInputIfNotValid(cmd.changeScene.path);
            meSendExternalCommand(cmd);
        }

        StringView scenePath = STRING_LIT("No Scene Loaded");
        meScene& scene = engine->sceneSystem->CurrentScene();
        MAID currentSceneAsset = scene.header;
        if (StringView currentScenePath = meAssetIndexGetFilesystemPath(currentSceneAsset))
        {
            scenePath = currentScenePath;
        }
		
		StringView rightAlignedText = StringFormatTmp("Avg framerate: %6.2f | %.*s | %.*s", 
            ImGui::GetIO().Framerate, STRING_VAARGS(engine->appConfig.appName), STRING_VAARGS(scenePath));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(rightAlignedText.cstr()).x
							 - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
		ImGui::TextEx(rightAlignedText.cstr());

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar();

}
