#include <imgui.h>
#include <fstream>
#include "Graphics/Graphics.h"
#include "GameObject/Components/ModelComponent.h"
#include "Core/ResourceManager.h"

ModelComponent::ModelComponent(const char* filename)
{
	modelFilename = filename;
	model = ResourceManager::Instance().LoadModel(filename);

	if (model != nullptr)
	{
		model->GetNodePoses(nodePoses);
		LoadMaterialSettings();
	}
}

void ModelComponent::PlayAnimation(const char* animName)
{
	if (model == nullptr) return;

	int newIndex = model->GetAnimationIndex(animName);
	if (newIndex >= 0 && animationIndex != newIndex)
	{
		animationIndex = newIndex;
		animationSeconds = 0.0f;
	}
}

void ModelComponent::Update(float elapsedTime)
{
	if (model != nullptr)
	{
		if (animationIndex >= 0)
		{
			const Model::Animation& anim = model->GetAnimations().at(animationIndex);

			model->ComputeAnimation(animationIndex, animationSeconds, nodePoses);
			model->SetNodePoses(nodePoses);

			animationSeconds += elapsedTime * animSpeed;
			if (animationSeconds > anim.secondsLength)
			{
				animationSeconds -= anim.secondsLength;
			}
		}

		std::shared_ptr<Actor> owner = GetActor();
		if (owner != nullptr)
		{
			model->UpdateTransform(owner->GetTransform());
		}
	}
}

void ModelComponent::Draw()
{
	if (!isVisible) return;

	if (model != nullptr)
	{
		ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
		if (renderer != nullptr)
		{
			renderer->Draw(shaderId, model);
		}
	}
}

void ModelComponent::SaveMaterialSettings()
{
	if (!model) return;

	std::string savePath = modelFilename + ".mat";
	std::ofstream ofs(savePath);
	if (!ofs) return;

	auto& materials = model->GetMaterials();

	ofs << materials.size() << "\n";

	for (const auto& m : materials)
	{
		ofs << m.baseColor.x << " " << m.baseColor.y << " " << m.baseColor.z << " " << m.baseColor.w << "\n";
		ofs << m.roughness << "\n";
		ofs << m.metalness << "\n";
		ofs << m.emissiveColor.x << " " << m.emissiveColor.y << " " << m.emissiveColor.z << "\n";
	}
}

void ModelComponent::LoadMaterialSettings()
{
	if (!model) return;

	std::string loadPath = modelFilename + ".mat";
	std::ifstream ifs(loadPath);
	if (!ifs) return;

	size_t count = 0;
	ifs >> count;

	auto& materials = model->GetMaterials();

	if (count != materials.size()) return;

	for (size_t i = 0; i < count; ++i)
	{
		auto& m = materials[i];
		ifs >> m.baseColor.x >> m.baseColor.y >> m.baseColor.z >> m.baseColor.w;
		ifs >> m.roughness;
		ifs >> m.metalness;
		ifs >> m.emissiveColor.x >> m.emissiveColor.y >> m.emissiveColor.z;
	}
}

void ModelComponent::OnGUI()
{
	if (model == nullptr)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"モデルが読み込まれていません");
		return;
	}

	int currentShader = static_cast<int>(shaderId);
	const char* shaderNames[] = { "Basic", "Lambert", "PBR", "Toon" };
	if (ImGui::Combo(u8"Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
	{
		shaderId = static_cast<ShaderId>(currentShader);
	}

	ImGui::Separator();
	ImGui::Text(u8"Animation");
	ImGui::SliderFloat(u8"Speed", &animSpeed, 0.0f, 5.0f);
	if (animationIndex >= 0)
	{
		ImGui::Text(u8"Index: %d  Time: %.2f", animationIndex, animationSeconds);
	}
	else
	{
		ImGui::TextDisabled(u8"アニメーションなし");
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader(u8"マテリアル (PBR / Toon)"))
	{
		if (ImGui::Button(u8"【 このモデルのマテリアル設定を保存する 】"))
		{
			SaveMaterialSettings();
		}
		ImGui::TextDisabled(u8"※保存ファイル: %s.mat", modelFilename.c_str());
		ImGui::Separator();

		auto& materials = model->GetMaterials();

		// =======================================================
		// ★プロのハック：全マテリアルのテクスチャを一括削除する機能
		// 誤爆防止のため、ImGuiの機能を使ってボタンを「警告の赤色」に染めています
		// =======================================================
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

		// ImVec2(-1, 0) を指定することで、ウィンドウの横幅いっぱいの巨大ボタンにする
		if (ImGui::Button(u8"【 全マテリアルのテクスチャを一括削除 (パージ) 】", ImVec2(-1, 0)))
		{
			for (auto& m : materials)
			{
				m.baseMap.Reset();
				m.metalnessRoughnessMap.Reset();
				m.normalMap.Reset();
				m.emissiveMap.Reset();
				m.occlusionMap.Reset();
			}
		}
		ImGui::PopStyleColor(3); // 変えた色を元に戻す
		ImGui::Separator();

		for (size_t i = 0; i < materials.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			Model::Material& m = materials[i];
			const char* label = m.name.empty() ? "Material" : m.name.c_str();
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen,
				u8"[%d] %s", (int)i, label))
			{
				ImGui::ColorEdit4(u8"ベースカラー", &m.baseColor.x);
				ImGui::SliderFloat(u8"粗さ (Roughness)", &m.roughness, 0.0f, 1.0f);
				ImGui::SliderFloat(u8"金属度 (Metalness)", &m.metalness, 0.0f, 1.0f);
				ImGui::ColorEdit3(u8"発光 (Emissive)", &m.emissiveColor.x);

				if (ImGui::SmallButton(u8"テクスチャ削除 (個別)"))
				{
					m.baseMap.Reset();
					m.metalnessRoughnessMap.Reset();
					m.normalMap.Reset();
					m.emissiveMap.Reset();
					m.occlusionMap.Reset();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
}