#include <imgui.h>
#include <fstream>
#include <cctype>
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
		CacheHeadNode();
		LoadMaterialSettings();
	}
}

void ModelComponent::SetModel(const char* filename)
{
	modelFilename = filename;
	animationIndex = -1;
	animationSeconds = 0.0f;
	nodePoses.clear();
	headNodeIndex = -1;
	headWasHidden = false;

	model = ResourceManager::Instance().LoadModel(filename);
	if (model != nullptr)
	{
		model->GetNodePoses(nodePoses);
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
	if (model == nullptr) return;

	bool poseDirty = false;

	if (animationIndex >= 0)
	{
		const Model::Animation& anim = model->GetAnimations().at(animationIndex);

		model->ComputeAnimation(animationIndex, animationSeconds, nodePoses);

		animationSeconds += elapsedTime * animSpeed;
		if (animationSeconds > anim.secondsLength)
		{
			animationSeconds -= anim.secondsLength;
		}
		poseDirty = true;
	}

	// Head hide (FPS): zero the head node scale so its mesh collapses away.
	if (headNodeIndex >= 0 && headNodeIndex < static_cast<int>(nodePoses.size()))
	{
		if (hideHead)
		{
			nodePoses[headNodeIndex].scale = { 0.0f, 0.0f, 0.0f };
			poseDirty = true;
		}
		else if (headWasHidden)
		{
			nodePoses[headNodeIndex].scale = headBaseScale;
			poseDirty = true;
		}
		headWasHidden = hideHead;
	}

	if (poseDirty)
	{
		model->SetNodePoses(nodePoses);
	}

	std::shared_ptr<Actor> owner = GetActor();
	if (owner != nullptr)
	{
		model->UpdateTransform(owner->GetTransform());
	}
}

void ModelComponent::OnEditorSync()
{
	if (model == nullptr) return;

	// Keep the head restored (or hidden) consistently in Edit mode, since
	// Update() is not ticked here.
	if (headNodeIndex >= 0 && headNodeIndex < static_cast<int>(nodePoses.size()))
	{
		nodePoses[headNodeIndex].scale = hideHead ? DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f } : headBaseScale;
		model->SetNodePoses(nodePoses);
		headWasHidden = hideHead;
	}

	std::shared_ptr<Actor> owner = GetActor();
	if (owner != nullptr)
	{
		model->UpdateTransform(owner->GetTransform());
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
	}

	// ---- Model swap (Inspector) --------------------------------------
	// Type a path and press Load, or pick a preset. Works for the Player
	// body, the ViewModel weapon, or any model actor.
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

		if (ImGui::TreeNode("Voxel characters (a-r)"))
		{
			for (char c = 'a'; c <= 'r'; ++c)
			{
				char path[128];
				sprintf_s(path, "Data/Model/GLB format/character-%c.glb", c);
				char label[32];
				sprintf_s(label, "character-%c", c);
				if (ImGui::Selectable(label))
				{
					SetModel(path);
					strncpy_s(pathBuf, path, _TRUNCATE);
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Weapons"))
		{
			const char* weapons[] = { "Pistol", "Rifle", "Staff", "ShootGun" };
			for (const char* w : weapons)
			{
				char path[128];
				sprintf_s(path, "Data/Model/Weapon/%s.glb", w);
				if (ImGui::Selectable(w))
				{
					SetModel(path);
					strncpy_s(pathBuf, path, _TRUNCATE);
				}
			}
			ImGui::TreePop();
		}
	}

	if (model == nullptr) return;

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
	if (animationIndex >= 0)
	{
		ImGui::Text("Index: %d  Time: %.2f", animationIndex, animationSeconds);
	}
	else
	{
		ImGui::TextDisabled("No animation");
	}

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