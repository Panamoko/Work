#include <imgui.h>
#include <fstream>
#include <cctype>
#include "Graphics/Graphics.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Actor.h"
#include "Core/ResourceManager.h"

ModelComponent::ModelComponent(const char* filename)
{
	SetModel(filename);
}

void ModelComponent::SetModel(const char* filename)
{
	modelFilename = filename;
	animationIndex = -1;
	animationSeconds = 0.0f;
	animatorDrove = false;
	headNodeIndex = -1;

	model = ResourceManager::Instance().LoadModel(filename);
	pose.SetModel(model);

	if (model != nullptr)
	{
		CacheHeadNode();
		LoadMaterialSettings();
	}
}

void ModelComponent::CacheHeadNode()
{
	headNodeIndex = -1;
	if (model == nullptr) return;

	const std::vector<Model::Node>& nodes = model->GetNodes();
	for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
	{
		std::string low = nodes[i].name;
		for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		if (low.find("head") != std::string::npos)
		{
			headNodeIndex = i;
			headBaseScale = nodes[i].scale;
			break;
		}
	}
}

void ModelComponent::SubmitAnimatorPose(const std::vector<Model::NodePose>& poses)
{
	pose.SetNodePoses(poses);
	animatorDrove = true;
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
	if (!pose.IsValid()) return;

	// If no AnimatorComponent fed us a pose this frame, run our own clip.
	if (!animatorDrove && animationIndex >= 0)
	{
		pose.ComputeAnimation(animationIndex, animationSeconds);

		const Model::Animation& anim = model->GetAnimations().at(animationIndex);
		animationSeconds += elapsedTime * animSpeed;
		if (animationSeconds > anim.secondsLength)
		{
			animationSeconds -= anim.secondsLength;
		}
	}

	// FPS head hide: collapse / restore the head node.
	if (headNodeIndex >= 0)
	{
		pose.SetNodeScale(headNodeIndex, hideHead ? DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f } : headBaseScale);
	}

	std::shared_ptr<Actor> owner = GetActor();
	if (owner != nullptr)
	{
		pose.UpdateTransform(owner->GetTransform());
	}

	animatorDrove = false;
}

void ModelComponent::OnEditorSync()
{
	if (!pose.IsValid()) return;

	if (headNodeIndex >= 0)
	{
		pose.SetNodeScale(headNodeIndex, hideHead ? DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f } : headBaseScale);
	}

	std::shared_ptr<Actor> owner = GetActor();
	if (owner != nullptr)
	{
		pose.UpdateTransform(owner->GetTransform());
	}
}

void ModelComponent::Draw()
{
	if (!isVisible) return;
	if (model == nullptr) return;

	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
	if (renderer != nullptr)
	{
		renderer->Draw(shaderId, model, &pose.GetNodes());
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
	// ---- Model file (Inspector): show current path, allow manual reload. ----
	// Preset lists intentionally removed here: use Settings > Player to swap
	// the body / weapon so you never replace the wrong actor by accident.
	if (ImGui::CollapsingHeader("Model file", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextWrapped("Current: %s", modelFilename.c_str());

		if (pathBuf[0] == '\0')
		{
			strncpy_s(pathBuf, modelFilename.c_str(), _TRUNCATE);
		}
		ImGui::InputText("Path", pathBuf, IM_ARRAYSIZE(pathBuf));
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			SetModel(pathBuf);
		}
	}

	if (model == nullptr)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model not loaded");
		return;
	}

	ImGui::Separator();

	int currentShader = static_cast<int>(shaderId);
	const char* shaderNames[] = { "Basic", "Lambert", "PBR", "Toon" };
	if (ImGui::Combo("Shader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames)))
	{
		shaderId = static_cast<ShaderId>(currentShader);
	}

	ImGui::Separator();

	ImGui::Text("Animation");
	ImGui::SliderFloat("Speed", &animSpeed, 0.0f, 5.0f);

	ImGui::Separator();

	if (headNodeIndex >= 0)
	{
		bool h = hideHead;
		if (ImGui::Checkbox("Hide head mesh (FPS)", &h)) hideHead = h;
	}
	else
	{
		ImGui::TextDisabled("No 'head' node in this model");
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