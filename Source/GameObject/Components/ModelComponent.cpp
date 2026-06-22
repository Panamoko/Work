#include <imgui.h>
#include "Graphics/Graphics.h"
#include "GameObject/Components/ModelComponent.h"
#include "Core/ResourceManager.h" // リソースマネージャー

ModelComponent::ModelComponent(const char* filename)
{
	// 毎回 new するのではなく、ResourceManager 経由で共有ポインタを取得する
	model = ResourceManager::Instance().LoadModel(filename);

	if (model != nullptr)
	{
		model->GetNodePoses(nodePoses);
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
	if (model != nullptr)
	{
		ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
		if (renderer != nullptr)
		{
			renderer->Draw(shaderId, model);
		}
	}
}

void ModelComponent::OnGUI()
{
	if (model != nullptr)
	{
		ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"モデル読込完了");

		int currentShader = static_cast<int>(shaderId);

		// =======================================================
		// ★修正：インスペクターのプルダウン配列に "Toon" を追加！
		// =======================================================
		const char* shaderNames[] = { "Basic", "Lambert", "PBR", "Toon" };
		if (ImGui::Combo("Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
		{
			shaderId = static_cast<ShaderId>(currentShader);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ マテリアル設定");
		auto& materials = model->GetMaterials();
		for (size_t i = 0; i < materials.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"マテリアル [%d]: %s", i, materials[i].name.c_str());

			if (ImGui::Button(u8"【 テクスチャの呪縛を解除 】")) {
				materials[i].baseMap.Reset();
				materials[i].metalnessRoughnessMap.Reset();
				materials[i].normalMap.Reset();
				materials[i].emissiveMap.Reset();
				materials[i].occlusionMap.Reset();
			}

			ImGui::SliderFloat(u8"粗さ (Roughness)", &materials[i].roughness, 0.0f, 1.0f);
			ImGui::SliderFloat(u8"金属度 (Metalness)", &materials[i].metalness, 0.0f, 1.0f);
			ImGui::ColorEdit3(u8"ベースカラー (色)", &materials[i].baseColor.x);

			ImGui::PopID();
			ImGui::Separator();
		}
	}
}