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
#include "asset/me_asset.h"

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

void meEditorInitialize(EngineContext* engine)
{
	engine->editor = MENEW(&engine->engineArena, EditorContext);

	meEventSubscribe(engine->assetSystem->assetBeginLoadingEvent, meEditorOnAssetBeginLoading);
	meEventSubscribe(engine->assetSystem->assetFinishedLoadingEvent, meEditorOnAssetFinishedLoading);

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
					meAsset newAsset = meAssetCreateNew(MAScene, openFilename);
					meJobId writeReq = meAssetRequestWrite(meSpanTyped<meAssetIdent>(&newAsset.ident, 1));
					UNUSED(writeReq); // don't need to wait on it...
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
                    meFsNormalizePathSeperators(sceneFile);
                    engine->sceneSystem->ChangeCurrentScene(sceneFile);
                }
            }
            if (ImGui::MenuItem("Save Current"))
            {
                meScene* currentScene = &engine->sceneSystem->CurrentScene();
				// a scene with an invalid header might mean a "untitled" scene
				// Like, when you first open the engine, we put you in a blank scene, and if you then make edits and save, it'll have stuff in it, but no asset header
				if (meAsset* asset = meAssetTryGet(currentScene->header))
				{
					if (!asset->ident.diskIdent)
					{
						auto openFileResult = pfd::open_file("Location to save the scene file", ".").result();
						ME_ASSERT(openFileResult.size() == 1);
						const char* fileCstr = openFileResult[0].c_str();
						StringView sceneFile = StringFromCString(fileCstr);
						meFsNormalizePathSeperators(sceneFile);
						asset->ident.diskIdent = sceneFile;
					}
					meAssetRequestWrite(meSpanTyped<meAssetIdent>(&currentScene->header, 1));
				}
            }
			if (ImGui::BeginMenu("Entity"))
			{
				if (ImGui::MenuItem("New"))
				{
					meScene* currentScene = &engine->sceneSystem->CurrentScene();
					EntityRef newEnt = Entity::CreateEntity(STRING_LIT("UnnamedEntity"));
					DynArrayPush(currentScene->entities, newEnt);
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