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

EditorContext& meEditorGetCtx()
{
	return *GetEngineCtx()->editor;
}

void meEditorOnAssetBeginLoading(meEventPayload payload)
{
	const meAssetIdent& ident = *(meAssetIdent*)payload.payload;
	ImGui::InsertNotification({ImGuiToastType::Info, 3000, "Began loading " STRING_FMT, STRING_VAARGS(ident.diskIdent)});
}

void meEditorOnAssetFinishedLoading(meEventPayload payload)
{
	const meAssetIdent& ident = *(meAssetIdent*)payload.payload;
	ImGui::InsertNotification({ImGuiToastType::Info, 3000, "Finished loading " STRING_FMT, STRING_VAARGS(ident.diskIdent)});
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
}

void meEditorTick(EngineContext* engine)
{
	EditorContext& editor = meEditorGetCtx();
	editor.editorCamera.UpdateCameraWithUserInput(*engine->osData);

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
				meScene* currentScene = &engine->sceneSystem->CurrentScene();
				if (meAsset* asset = meAssetTryGet(currentScene->header))
				{
					if (!asset->ident.diskIdent)
					{
						openFileResult = pfd::open_file("Location to save the scene file", ".").result();
						if (!openFileResult.empty())
						{
							ME_ASSERT(openFileResult.size() == 1);
							const char* fileCstr = openFileResult[0].c_str();
							cmd.saveCurrentScene.path = StringFromCString(fileCstr);
						}
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