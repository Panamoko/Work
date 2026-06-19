#include <imgui.h>
#include "Graphics/Graphics.h"
#include "GameObject/Components/ModelComponent.h"
#include "Core/ResourceManager.h" // ★プロの追加：リソースマネージャーをインクルード

ModelComponent::ModelComponent(const char* filename)
{
	// ★プロの修正：毎回 new するのではなく、ResourceManager 経由で共有ポインタを取得する（Flyweightパターン）
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
		const char* shaderNames[] = { "Basic", "Lambert" };
		if (ImGui::Combo("Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
		{
			shaderId = static_cast<ShaderId>(currentShader);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"アニメーション設定");

		// ★プロのハック: バー付きのスライダー (SliderFloat) を使用。
		// アニメーション速度のように「下限(0.0)と上限(5.0)が明確な値」にはこちらが最適です。
		ImGui::SliderFloat("Speed", &animSpeed, 0.0f, 5.0f);

		if (animationIndex >= 0)
		{
			ImGui::Text("Current Anim Index: %d", animationIndex);
			ImGui::Text("Time: %.2f", animationSeconds);
		}
		else
		{
			ImGui::Text("No Animation Playing");
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"モデルがありません");
	}
}