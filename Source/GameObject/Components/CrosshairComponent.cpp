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
	if (dynamicOffset > 0.0f) {
		dynamicOffset -= offsetRecoverySpeed * elapsedTime;
		if (dynamicOffset < 0.0f) dynamicOffset = 0.0f;
	}
}

void CrosshairComponent::OnGUI()
{
	if (isEnabled) {
		// =======================================================
		// ★完全解決策：メインビューポート（ゲーム画面）の情報を取得
		// =======================================================
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		if (viewport->Size.x > 0 && viewport->Size.y > 0) {

			// ★モニター座標ではなく、「ゲーム画面の座標とサイズ」にピッタリ合わせる
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);

			// ★最重要ハック：このウィンドウがゲーム画面の外（別ウィンドウ）に飛び出すのを禁止する
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			// 背景透明、入力無視の安全なHUDウィンドウを作成
			ImGui::Begin("CrosshairHUD", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			// 中心座標の計算（ウィンドウの左上座標 ＋ 幅・高さの半分）
			float cx = viewport->Pos.x + (viewport->Size.x * 0.5f);
			float cy = viewport->Pos.y + (viewport->Size.y * 0.5f);

			ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(crosshairColor[0], crosshairColor[1], crosshairColor[2], crosshairColor[3]));
			float totalOffset = innerLineOffset + dynamicOffset;

			// 3. センタードットの描画
			if (showCenterDot) {
				float halfSize = centerDotSize * 0.5f;
				drawList->AddRectFilled(ImVec2(cx - halfSize, cy - halfSize), ImVec2(cx + halfSize, cy + halfSize), color);
			}

			// 4. 上下左右のラインの描画
			float halfThick = innerLineThickness * 0.5f;
			drawList->AddRectFilled(ImVec2(cx - halfThick, cy - totalOffset - innerLineLength), ImVec2(cx + halfThick, cy - totalOffset), color);
			drawList->AddRectFilled(ImVec2(cx - halfThick, cy + totalOffset), ImVec2(cx + halfThick, cy + totalOffset + innerLineLength), color);
			drawList->AddRectFilled(ImVec2(cx - totalOffset - innerLineLength, cy - halfThick), ImVec2(cx - totalOffset, cy + halfThick), color);
			drawList->AddRectFilled(ImVec2(cx + totalOffset, cy - halfThick), ImVec2(cx + totalOffset + innerLineLength, cy + halfThick), color);

			ImGui::End();
			ImGui::PopStyleVar(2);
		}
	}

	// =======================================================
	// インスペクタ（操作パネル）
	// =======================================================
	ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"▼ クロスヘア設定 (Crosshair)");
	if (ImGui::Button(u8"【 設定を保存する 】##Crosshair")) SaveSettings();
	ImGui::Separator();

	ImGui::Checkbox(u8"クロスヘアを表示", &isEnabled);
	if (isEnabled) {
		ImGui::ColorEdit4(u8"色 (Color)", crosshairColor);

		ImGui::Separator();
		ImGui::Checkbox(u8"センタードット", &showCenterDot);
		if (showCenterDot) ImGui::SliderFloat(u8"ドットの大きさ", &centerDotSize, 1.0f, 10.0f, "%.1f");

		ImGui::Separator();
		ImGui::SliderFloat(u8"インナーライン 太さ", &innerLineThickness, 1.0f, 20.0f, "%.1f");
		ImGui::SliderFloat(u8"インナーライン 長さ", &innerLineLength, 0.0f, 50.0f, "%.1f");
		ImGui::SliderFloat(u8"インナーライン 開き(オフセット)", &innerLineOffset, 0.0f, 50.0f, "%.1f");

		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"テスト: 動的オフセット (反動エミュレーション)");
		ImGui::SliderFloat(u8"反動の回復速度", &offsetRecoverySpeed, 1.0f, 50.0f, "%.1f");
		if (ImGui::Button(u8"射撃テスト (クリックで開く)")) {
			AddDynamicOffset(15.0f);
		}
	}
}