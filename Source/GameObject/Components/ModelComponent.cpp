#include <imgui.h>
#include "Graphics/Graphics.h"
#include "GameObject/Components/ModelComponent.h"
#include "Core/ResourceManager.h" // ���v���̒ǉ��F���\�[�X�}�l�[�W���[���C���N���[�h

ModelComponent::ModelComponent(const char* filename)
{
	// ���v���̏C���F���� new ����̂ł͂Ȃ��AResourceManager �o�R�ŋ��L�|�C���^���擾����iFlyweight�p�^�[���j
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
		ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"���f���Ǎ�����");

		int currentShader = static_cast<int>(shaderId);
		const char* shaderNames[] = { "Basic", "Lambert","PBR"};
		if (ImGui::Combo("Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
		{
			shaderId = static_cast<ShaderId>(currentShader);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"�A�j���[�V�����ݒ�");

		// ���v���̃n�b�N: �o�[�t���̃X���C�_�[ (SliderFloat) ���g�p�B
		// �A�j���[�V�������x�̂悤�Ɂu����(0.0)�Ə��(5.0)�����m�Ȓl�v�ɂ͂����炪�œK�ł��B
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

		// ===== Material editor (PBR) =====
		ImGui::Separator();
		if (ImGui::CollapsingHeader(u8"Materials (PBR)"))
		{
			std::vector<Model::Material>& materials = model->GetMaterials();
			for (size_t i = 0; i < materials.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				Model::Material& m = materials[i];
				if (ImGui::TreeNodeEx(m.name.c_str(),
					ImGuiTreeNodeFlags_DefaultOpen, u8"[%d] %s", (int)i, m.name.c_str()))
				{
					ImGui::ColorEdit4(u8"Base Color", &m.baseColor.x);
					ImGui::SliderFloat(u8"Roughness", &m.roughness, 0.0f, 1.0f);
					ImGui::SliderFloat(u8"Metalness", &m.metalness, 0.0f, 1.0f);
					ImGui::ColorEdit3(u8"Emissive", &m.emissiveColor.x);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"���f��������܂���");
	}
}