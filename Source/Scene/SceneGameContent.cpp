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

// ---- Content drawer ----
void SceneGame::ScanAssets()
{
	assetPaths.clear();
	namespace fs = std::filesystem;
	const char* root = "Data/Model";
	std::error_code ec;
	if (fs::exists(root, ec))
	{
		for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
		{
			if (!it->is_regular_file()) continue;
			std::string ext = it->path().extension().string();
			for (char& ch : ext) ch = (char)tolower((unsigned char)ch);
			if (ext == ".glb" || ext == ".gltf")
			{
				std::string p = it->path().string();
				for (char& ch : p) if (ch == '\\') ch = '/';
				assetPaths.push_back(p);
			}
		}
	}
	std::sort(assetPaths.begin(), assetPaths.end());
	BuildAssetTree();
}

void SceneGame::SpawnAsset(const std::string& path)
{
	DirectX::XMFLOAT3 eye = camera.GetEye();
	DirectX::XMFLOAT3 fwd = camera.GetFront();
	DirectX::XMFLOAT3 pos = { eye.x + fwd.x * 6.0f, eye.y + fwd.y * 6.0f, eye.z + fwd.z * 6.0f };

	if (!stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 o = { pos.x, pos.y + 20.0f, pos.z };
		DirectX::XMFLOAT3 down = { 0.0f, -1.0f, 0.0f };
		float d = 0.0f; DirectX::XMFLOAT3 hp, hn;
		if (stageCollision.RayCast(o, down, 100.0f, d, hp, hn))
			pos.y = hp.y;
	}

	std::shared_ptr<class Actor> a = ActorManager::Instance().Create();
	std::string name = std::filesystem::path(path).stem().string();
	a->SetName(name.c_str());
	auto mc = a->AddComponent<ModelComponent>(path.c_str());
	mc->SetShaderId(ShaderId::Toon);
	a->SetPosition(pos);

	if (spawnType == 1) // Enemy: attach AI
	{
		a->AddComponent<AnimatorComponent>();
		auto ec = a->AddComponent<EnemyComponent>(playerActor);
		ec->SetStageCollision(stageCollision);
		ec->SetNavGrid(&navGrid);
	}

	spawnedActors.push_back(a);
}

// ---- Scene save / load ----
void SceneGame::SaveScene()
{
	std::ofstream ofs("Scene.txt");
	if (!ofs) return;

	for (std::shared_ptr<class Actor>& a : spawnedActors)
	{
		if (!a) continue;
		auto mc = a->GetComponent<ModelComponent>();
		if (!mc) continue;
		const std::string& path = mc->GetModelPath();
		if (path.empty()) continue;

		int type = a->GetComponent<EnemyComponent>() ? 1 : 0;

		const DirectX::XMFLOAT3& p = a->GetPosition();
		const DirectX::XMFLOAT4& r = a->GetRotation();
		const DirectX::XMFLOAT3& s = a->GetScale();
		ofs << type << " "
			<< p.x << " " << p.y << " " << p.z << " "
			<< r.x << " " << r.y << " " << r.z << " " << r.w << " "
			<< s.x << " " << s.y << " " << s.z << " "
			<< path << "\n";
	}
}

void SceneGame::LoadScene()
{
	std::ifstream ifs("Scene.txt");
	if (!ifs) return;

	ClearScene();

	int type;
	DirectX::XMFLOAT3 p, s;
	DirectX::XMFLOAT4 r;
	while (ifs >> type >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z >> r.w >> s.x >> s.y >> s.z)
	{
		std::string path;
		std::getline(ifs, path);
		size_t st = path.find_first_not_of(" \t");
		if (st == std::string::npos) continue;
		path = path.substr(st);
		if (path.empty()) continue;

		std::shared_ptr<class Actor> a = ActorManager::Instance().Create();
		std::string name = std::filesystem::path(path).stem().string();
		a->SetName(name.c_str());
		auto mc = a->AddComponent<ModelComponent>(path.c_str());
		mc->SetShaderId(ShaderId::Toon);
		a->SetPosition(p);
		a->SetRotation(r);
		a->SetScale(s);

		if (type == 1)
		{
			a->AddComponent<AnimatorComponent>();
			auto ec = a->AddComponent<EnemyComponent>(playerActor);
			ec->SetStageCollision(stageCollision);
			ec->SetNavGrid(&navGrid);
		}

		spawnedActors.push_back(a);
	}
}

void SceneGame::ClearScene()
{
	for (std::shared_ptr<class Actor>& a : spawnedActors)
		if (a) ActorManager::Instance().Remove(a);
	spawnedActors.clear();
}


void SceneGame::BuildAssetTree()
{
	assetTree = AssetNode{};
	const std::string prefix = "Data/Model/";
	for (const std::string& full : assetPaths)
	{
		std::string rel = full;
		if (rel.rfind(prefix, 0) == 0) rel = rel.substr(prefix.size());

		AssetNode* node = &assetTree;
		size_t start = 0;
		while (true)
		{
			size_t slash = rel.find('/', start);
			if (slash == std::string::npos)
			{
				node->files.push_back(full);
				break;
			}
			std::string folder = rel.substr(start, slash - start);
			node = &node->folders[folder];
			start = slash + 1;
		}
	}
}

void SceneGame::DrawAssetNode(const AssetNode& node, const char* filter)
{
	// Folders first.
	for (const auto& kv : node.folders)
	{
		if (ImGui::TreeNodeEx(kv.first.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			DrawAssetNode(kv.second, filter);
			ImGui::TreePop();
		}
	}
	// Files.
	for (const std::string& p : node.files)
	{
		std::string fn = std::filesystem::path(p).filename().string();
		if (filter && filter[0] != '\0' && fn.find(filter) == std::string::npos) continue;
		bool active = (p == activeTilePath);
		if (ImGui::Selectable(fn.c_str(), active))
		{
			if (paintMode) activeTilePath = p;
			else SpawnAsset(p);
		}
	}
}