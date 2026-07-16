#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h"
#include "Scene/SceneGame.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/ViewModelComponent.h"
#include "GameObject/Components/CrosshairComponent.h"
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/HealthComponent.h"
#include "Core/Input.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cfloat>
#include <windows.h>
#pragma comment(lib, "d3dcompiler.lib")

void SceneGame::DrawGUI()
{
	ImGuiIO& io = ImGui::GetIO();
	const float topH = 38.0f, leftW = 300.0f, rightW = 340.0f, bottomH = 240.0f;
	const float hierH = (io.DisplaySize.y - topH) * 0.55f;

	// ---- Top toolbar ----
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, topH), ImGuiCond_Always);
	ImGui::Begin("##Toolbar", nullptr,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	if (mode == EditorMode::Edit)
	{
		if (ImGui::Button("  Play  ")) StartPlay();
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "EDIT");
		ImGui::SameLine();
		ImGui::TextDisabled("(hold RMB + WASD / E / Q to fly, Shift = fast)");
		ImGui::SameLine();
		const char* mm = (gizmoMode == 0) ? "Move (W)" : (gizmoMode == 1) ? "Rotate (E)" : "Scale (R)";
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "| Gizmo: %s", mm);
	}
	else
	{
		if (ImGui::Button("  Stop  ")) StopPlay();
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "PLAY");
	}
	ImGui::End();

	// ---- Settings panel (left, lower) ----
	ImGui::SetNextWindowPos(ImVec2(0.0f, topH + hierH), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(leftW, io.DisplaySize.y - topH - hierH), ImGuiCond_Always);
	ImGui::Begin("Settings", nullptr,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();

	if (ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Paint Mode", &paintMode);
		ImGui::SameLine();
		ImGui::Checkbox("Erase", &eraseMode);
		ImGui::SliderFloat("Tile Size", &tileSize, 0.5f, 8.0f);
		ImGui::SliderFloat("Paint Height Y", &paintY, -10.0f, 20.0f);
		std::string act = activeTilePath.empty() ? "(none)" : std::filesystem::path(activeTilePath).filename().string();
		ImGui::Text("Active tile: %s", act.c_str());
		ImGui::TextDisabled(paintMode ? "Click in the viewport to place tiles" : "Enable Paint Mode, then pick a tile below");
		ImGui::Text("Placed tiles: %d", (int)tiles.size());
		if (ImGui::Button("Save Tiles")) SaveTilemap();
		ImGui::SameLine();
		if (ImGui::Button("Load Tiles")) LoadTilemap();
		ImGui::SameLine();
		if (ImGui::Button("Clear Tiles")) ClearTilemap();
	}

	if (ImGui::CollapsingHeader("Scene (placed objects)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Placed objects: %d", (int)spawnedActors.size());
		if (ImGui::Button("Save Scene")) SaveScene();
		ImGui::SameLine();
		if (ImGui::Button("Load Scene")) LoadScene();
		ImGui::SameLine();
		if (ImGui::Button("Clear Scene")) ClearScene();
	}

	if (ImGui::CollapsingHeader("Player"))
	{
		auto pc = playerActor ? playerActor->GetComponent<PlayerControllerComponent>() : nullptr;
		auto bodyMc = playerActor ? playerActor->GetComponent<ModelComponent>() : nullptr;
		auto vmMc = viewModelActor ? viewModelActor->GetComponent<ModelComponent>() : nullptr;

		ImGui::TextDisabled("Movement preset");
		if (pc)
		{
			if (ImGui::Button("Light"))  pc->SetMoveSpeeds(5.0f, 10.0f);
			ImGui::SameLine();
			if (ImGui::Button("Normal")) pc->SetMoveSpeeds(4.0f, 8.0f);
			ImGui::SameLine();
			if (ImGui::Button("Heavy"))  pc->SetMoveSpeeds(3.0f, 5.5f);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Player body model  (name contains 'character')");
		if (bodyMc)
		{
			std::string cur = std::filesystem::path(bodyMc->GetModelPath()).filename().string();
			ImGui::Text("Current: %s", cur.c_str());
		}
		ImGui::BeginChild("bodylist", ImVec2(0, 100));
		for (const std::string& p : assetPaths)
		{
			std::string fn = std::filesystem::path(p).filename().string();
			std::string fnLow = fn;
			for (char& ch : fnLow) ch = (char)tolower((unsigned char)ch);
			if (fnLow.find("character") == std::string::npos) continue;
			if (ImGui::Selectable(fn.c_str()))
			{
				if (bodyMc) bodyMc->SetModel(p.c_str());
			}
		}
		ImGui::EndChild();

		ImGui::Separator();
		ImGui::TextDisabled("Weapon (first-person)  (Data/Model/Weapon/*)");
		if (vmMc)
		{
			std::string cur = std::filesystem::path(vmMc->GetModelPath()).filename().string();
			ImGui::Text("Current: %s", cur.c_str());
		}
		ImGui::BeginChild("weaponlist", ImVec2(0, 90));
		for (const std::string& p : assetPaths)
		{
			std::string low = p;
			for (char& ch : low) ch = (char)tolower((unsigned char)ch);
			if (low.find("/weapon/") == std::string::npos) continue;
			std::string fn = std::filesystem::path(p).filename().string();
			if (ImGui::Selectable(fn.c_str()))
			{
				if (vmMc) vmMc->SetModel(p.c_str());
			}
		}
		ImGui::EndChild();
	}

	auto applyLook = [&](
		float zx, float zy, float zz, float hx, float hy, float hz, float gx, float gy, float gz,
		float hExp, float bands,
		float exposure, float contrast, float saturation, float bThresh, float bInt,
		float vig, float vigR, float tr, float tg, float tb, float shadow)
		{
			skybox.zenith = { zx, zy, zz };
			skybox.horizon = { hx, hy, hz };
			skybox.ground = { gx, gy, gz };
			skybox.horizonExp = hExp;
			skybox.bands = bands;
			Graphics& g = Graphics::Instance();
			g.postExposure = exposure;
			g.postContrast = contrast;
			g.postSaturation = saturation;
			g.postTonemap = true;
			g.postBloomEnable = true;
			g.postBloomThreshold = bThresh;
			g.postBloomIntensity = bInt;
			g.postVignette = vig;
			g.postVignetteRadius = vigR;
			g.postTint[0] = tr; g.postTint[1] = tg; g.postTint[2] = tb;
			renderer->shadowStrength = shadow;
		};

	if (ImGui::CollapsingHeader("Art Style (Presets)"))
	{
		if (ImGui::Button("Anime Bright"))
			applyLook(0.35f, 0.62f, 0.95f, 0.85f, 0.93f, 1.00f, 0.45f, 0.50f, 0.55f,
				0.50f, 0.0f, 1.20f, 1.05f, 1.20f, 1.10f, 0.50f, 0.15f, 0.70f, 1.00f, 1.00f, 1.00f, 0.55f);
		ImGui::SameLine();
		if (ImGui::Button("Cinematic Dusk"))
			applyLook(0.25f, 0.30f, 0.55f, 0.95f, 0.60f, 0.40f, 0.20f, 0.18f, 0.20f,
				0.70f, 0.0f, 1.10f, 1.15f, 1.10f, 0.90f, 0.80f, 0.45f, 0.50f, 1.05f, 0.95f, 0.85f, 0.30f);
		if (ImGui::Button("Cool Stylized"))
			applyLook(0.20f, 0.40f, 0.70f, 0.70f, 0.85f, 0.95f, 0.25f, 0.30f, 0.35f,
				0.60f, 4.0f, 1.00f, 1.10f, 1.00f, 1.20f, 0.50f, 0.30f, 0.60f, 0.92f, 0.97f, 1.05f, 0.40f);
		ImGui::SameLine();
		if (ImGui::Button("Neutral"))
			applyLook(0.18f, 0.42f, 0.82f, 0.80f, 0.88f, 0.96f, 0.32f, 0.34f, 0.38f,
				0.55f, 0.0f, 1.00f, 1.00f, 1.00f, 1.00f, 0.60f, 0.35f, 0.60f, 1.00f, 1.00f, 1.00f, 0.35f);
	}

	ImGui::Checkbox("IBL (Image Based Lighting)", &renderer->enableIBL);

	if (ImGui::CollapsingHeader("Post Process"))
	{
		ImGui::Checkbox("Tone Mapping (ACES)", &Graphics::Instance().postTonemap);
		ImGui::SliderFloat("Exposure", &Graphics::Instance().postExposure, 0.1f, 4.0f);
		ImGui::SliderFloat("Contrast", &Graphics::Instance().postContrast, 0.5f, 2.0f);
		ImGui::SliderFloat("Saturation", &Graphics::Instance().postSaturation, 0.0f, 2.0f);
		ImGui::Separator();
		ImGui::Checkbox("Bloom", &Graphics::Instance().postBloomEnable);
		ImGui::SliderFloat("Bloom Threshold", &Graphics::Instance().postBloomThreshold, 0.0f, 3.0f);
		ImGui::SliderFloat("Bloom Intensity", &Graphics::Instance().postBloomIntensity, 0.0f, 2.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Vignette", &Graphics::Instance().postVignette, 0.0f, 1.0f);
		ImGui::SliderFloat("Vignette Start", &Graphics::Instance().postVignetteRadius, 0.2f, 1.0f);
		ImGui::ColorEdit3("Color Filter (Tint)", Graphics::Instance().postTint);
	}

	if (ImGui::CollapsingHeader("Shadow"))
	{
		ImGui::Checkbox("Enable Shadow", &renderer->enableShadow);
		ImGui::SliderFloat("Shadow Brightness", &renderer->shadowStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("Shadow Bias", &renderer->shadowBias, 0.0f, 0.01f, "%.4f");
		ImGui::SliderFloat("Shadow Radius", &renderer->shadowRadius, 5.0f, 80.0f);
		ImGui::SliderFloat("Light Distance", &renderer->shadowDistance, 20.0f, 200.0f);
	}

	if (ImGui::CollapsingHeader("Sky (Gradient)"))
	{
		ImGui::ColorEdit3("Zenith (top)", &skybox.zenith.x);
		ImGui::ColorEdit3("Horizon (mid)", &skybox.horizon.x);
		ImGui::ColorEdit3("Ground (bottom)", &skybox.ground.x);
		ImGui::SliderFloat("Horizon Width", &skybox.horizonExp, 0.05f, 2.0f);
		ImGui::SliderFloat("Toon Bands (0=off)", &skybox.bands, 0.0f, 16.0f, "%.0f");
	}

	if (ImGui::CollapsingHeader("NavMesh"))
	{
		ImGui::Text("Built : %s", navGrid.IsBuilt() ? "YES" : "no");
		ImGui::Text("Cells : %d   Reachable : %d", navGrid.CellCount(), navGrid.ReachableCount());
		ImGui::Checkbox("Show NavMesh", &showNav);
		ImGui::SliderFloat("Debug Radius", &navDebugRadius, 3.0f, 80.0f);
		ImGui::SliderFloat("Nav Cell Size", &navCellSize, 0.3f, 4.0f);
		ImGui::SliderFloat("Max Step", &navMaxStep, 0.1f, 2.0f);
		if (ImGui::Button("Rebuild NavMesh")) BuildNavGrid();
	}

	ImGui::End();

	// ---- Content Drawer (bottom center) ----
	ImGui::SetNextWindowPos(ImVec2(leftW, io.DisplaySize.y - bottomH), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - leftW - rightW, bottomH), ImGuiCond_Always);
	ImGui::Begin("Content Drawer", nullptr,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	if (ImGui::Button("Rescan")) ScanAssets();
	ImGui::SameLine();
	ImGui::Text("%d assets", (int)assetPaths.size());
	ImGui::SameLine();
	ImGui::PushItemWidth(120.0f);
	const char* spawnTypes[] = { "Prop", "Enemy" };
	ImGui::Combo("Spawn as", &spawnType, spawnTypes, 2);
	ImGui::PopItemWidth();
	static char assetFilter[128] = "";
	ImGui::PushItemWidth(-1);
	ImGui::InputTextWithHint("##assetfilter", "Search...", assetFilter, sizeof(assetFilter));
	ImGui::PopItemWidth();
	ImGui::TextDisabled(paintMode ? "Click a tile to set it ACTIVE, then paint in the viewport"
		: "Click a model to place it (Spawn as: Prop / Enemy)");
	ImGui::Separator();
	ImGui::BeginChild("assetlist");
	DrawAssetNode(assetTree, assetFilter);
	ImGui::EndChild();
	ImGui::End();

	// ---- ImGuizmo ----
	DrawImGuizmo();
}

