#include "me_editor.h"
#include "external/imgui/imgui.h"

void meEditorInitialize(EngineContext* ctx)
{

}

void meEditorDisplayGui(EngineContext* ctx)
{
	ctx->renderer->BeginImguiContext();

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	if (ImGui::BeginMainMenuBar())
	{
		ImGui::Text("%.*s", STRING_VAARGS(GetEngineCtx()->appConfig->appName));
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Button1"))
			{ 
				
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) 
			{
				
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("AnotherMenu"))
		{

			ImGui::EndMenu();
		}
		StringView fpsText = StringFormat("Avg framerate: %6.2f", ImGui::GetIO().Framerate);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(fpsText.cstr()).x 
							 - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
		ImGui::TextEx(fpsText.cstr());
		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar();


	ctx->renderer->EndImguiContext();
}