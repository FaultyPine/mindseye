#include "me_editor.h"
#include "external/imgui/imgui.h"
#include "external/potable-file-dialogs.h"

void meEditorInitialize(EngineContext* ctx)
{

}

void meEditorDisplayGui(EngineContext* ctx)
{
	ctx->renderer->BeginImguiContext();

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::MenuItem("Open", "CTRL+O"))
		{
			auto openFileResult = pfd::open_file("Select a scene file", ".", { "*.scn" }).result();
			if (!openFileResult.empty())
			{
				ME_ASSERT(openFileResult.size() == 1);
				StringView sceneFile = StringFromCString(openFileResult[0].c_str());
				meFsNormalizePathSeperators(sceneFile);
				ctx->sceneSystem->LoadSceneFromFileBlocking(sceneFile, &ctx->engineSceneAllocator, &ctx->sceneSystem->scene);
			}
		}
		StringView rightAlignedText = StringFormat("Avg framerate: %6.2f | %.*s", ImGui::GetIO().Framerate, STRING_VAARGS(ctx->appConfig->appName));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(rightAlignedText.cstr()).x
							 - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
		ImGui::TextEx(rightAlignedText.cstr());

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar();


	ctx->renderer->EndImguiContext();
}