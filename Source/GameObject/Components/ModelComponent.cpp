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

		// Keep the model's original textures.
		// (The old "purge all textures on load" hack was removed; that was
		//  what made Helmet / Player turn white.)
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
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model not loaded");
		return;
	}

	int currentShader = static_cast<int>(shaderId);
	const char* shaderNames[] = { "Basic", "Lambert", "PBR", "Toon" };
	if (ImGui::Combo("Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
	{
		shaderId = static_cast<ShaderId>(currentShader);
	}

	ImGui::Separator();
	ImGui::Text("Animation");
	ImGui::SliderFloat("Speed", &animSpeed, 0.0f, 5.0f);
	if (animationIndex >= 0)
	{
		ImGui::Text("Index: %d  Time: %.2f", animationIndex, animationSeconds);
	}
	else
	{
		ImGui::TextDisabled("No animation");
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Materials (PBR / Toon)"))
	{
		if (ImGui::Button("Save material settings (.mat)"))
		{
			SaveMaterialSettings();
		}
		ImGui::TextDisabled("Save file: %s.mat", modelFilename.c_str());
		ImGui::Separator();

		auto& materials = model->GetMaterials();

		for (size_t i = 0; i < materials.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			Model::Material& m = materials[i];
			const char* label = m.name.empty() ? "Material" : m.name.c_str();
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen, "[%d] %s", (int)i, label))
			{
				ImGui::ColorEdit4("Base Color", &m.baseColor.x);
				ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
				ImGui::SliderFloat("Metalness", &m.metalness, 0.0f, 1.0f);
				ImGui::ColorEdit3("Emissive", &m.emissiveColor.x);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
}