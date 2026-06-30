#include <imgui.h>
#include <fstream>
#include "GameObject/Components/CrosshairComponent.h"

void CrosshairComponent::Start()
{
	LoadSettings();
}

void CrosshairComponent::SaveSettings()
{
	std::ofstream ofs("CrosshairSettings.txt");
	if (ofs) {
		ofs << isEnabled << " " << showCenterDot << "\n";
		ofs << centerDotSize << " " << innerLineThickness << " " << innerLineLength << " " << innerLineOffset << "\n";
		ofs << crosshairColor[0] << " " << crosshairColor[1] << " " << crosshairColor[2] << " " << crosshairColor[3] << "\n";
	}
}

void CrosshairComponent::LoadSettings()
{
	std::ifstream ifs("CrosshairSettings.txt");
	if (ifs) {
		ifs >> isEnabled >> showCenterDot;
		ifs >> centerDotSize >> innerLineThickness >> innerLineLength >> innerLineOffset;
		ifs >> crosshairColor[0] >> crosshairColor[1] >> crosshairColor[2] >> crosshairColor[3];
	}
}

void CrosshairComponent::Update(float elapsedTime)
{
	// Recover the dynamic (recoil) offset over time.
	if (dynamicOffset > 0.0f) {
		dynamicOffset -= offsetRecoverySpeed * elapsedTime;
		if (dynamicOffset < 0.0f) dynamicOffset = 0.0f;
	}

	// ---- Draw the crosshair HUD every frame (always on) ----
	if (!isEnabled) return;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (viewport->Size.x <= 0 || viewport->Size.y <= 0) return;

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("CrosshairHUD", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	float cx = viewport->Pos.x + (viewport->Size.x * 0.5f);
	float cy = viewport->Pos.y + (viewport->Size.y * 0.5f);

	ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(crosshairColor[0], crosshairColor[1], crosshairColor[2], crosshairColor[3]));
	float totalOffset = innerLineOffset + dynamicOffset;

	if (showCenterDot) {
		float halfSize = centerDotSize * 0.5f;
		drawList->AddRectFilled(ImVec2(cx - halfSize, cy - halfSize), ImVec2(cx + halfSize, cy + halfSize), color);
	}

	float halfThick = innerLineThickness * 0.5f;
	drawList->AddRectFilled(ImVec2(cx - halfThick, cy - totalOffset - innerLineLength), ImVec2(cx + halfThick, cy - totalOffset), color);
	drawList->AddRectFilled(ImVec2(cx - halfThick, cy + totalOffset), ImVec2(cx + halfThick, cy + totalOffset + innerLineLength), color);
	drawList->AddRectFilled(ImVec2(cx - totalOffset - innerLineLength, cy - halfThick), ImVec2(cx - totalOffset, cy + halfThick), color);
	drawList->AddRectFilled(ImVec2(cx + totalOffset, cy - halfThick), ImVec2(cx + totalOffset + innerLineLength, cy + halfThick), color);

	ImGui::End();
	ImGui::PopStyleVar(2);
}

void CrosshairComponent::OnGUI()
{
	// Inspector controls only (HUD is drawn every frame in Update).
	ImGui::TextColored(ImVec4(0, 1, 0, 1), "Crosshair Settings");
	if (ImGui::Button("Save Settings##Crosshair")) SaveSettings();
	ImGui::Separator();

	ImGui::Checkbox("Show Crosshair", &isEnabled);
	if (isEnabled) {
		ImGui::ColorEdit4("Color", crosshairColor);

		ImGui::Separator();
		ImGui::Checkbox("Center Dot", &showCenterDot);
		if (showCenterDot) ImGui::SliderFloat("Dot Size", &centerDotSize, 1.0f, 10.0f, "%.1f");

		ImGui::Separator();
		ImGui::SliderFloat("Line Thickness", &innerLineThickness, 1.0f, 20.0f, "%.1f");
		ImGui::SliderFloat("Line Length", &innerLineLength, 0.0f, 50.0f, "%.1f");
		ImGui::SliderFloat("Line Gap (Offset)", &innerLineOffset, 0.0f, 50.0f, "%.1f");

		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Test: dynamic offset (recoil)");
		ImGui::SliderFloat("Recovery Speed", &offsetRecoverySpeed, 1.0f, 50.0f, "%.1f");
		if (ImGui::Button("Fire Test")) {
			AddDynamicOffset(15.0f);
		}
	}
}