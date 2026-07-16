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

// ---- Tilemap ----
bool SceneGame::ScreenRay(DirectX::XMFLOAT3& outOrigin, DirectX::XMFLOAT3& outDir) const
{
	float W = Graphics::Instance().GetScreenWidth();
	float H = Graphics::Instance().GetScreenHeight();
	if (W <= 0.0f || H <= 0.0f) return false;

	Mouse& mouse = Input::Instance().GetMouse();
	float mx = static_cast<float>(mouse.GetPositionX());
	float my = static_cast<float>(mouse.GetPositionY());

	using namespace DirectX;

	// Build view / projection EXACTLY like Render() so the ray matches the pixels.
	XMVECTOR Rv = XMLoadFloat3(&camera.GetRight());
	XMVECTOR Uv = XMLoadFloat3(&camera.GetUp());
	XMVECTOR Fv = XMLoadFloat3(&camera.GetFront());
	XMVECTOR Ev = XMLoadFloat3(&camera.GetEye());
	XMMATRIX camWorld;
	camWorld.r[0] = XMVectorSetW(Rv, 0.0f);
	camWorld.r[1] = XMVectorSetW(Uv, 0.0f);
	camWorld.r[2] = XMVectorSetW(Fv, 0.0f);
	camWorld.r[3] = XMVectorSetW(Ev, 1.0f);
	XMMATRIX view = XMMatrixInverse(nullptr, camWorld);

	float aspect = W / H;
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60), aspect, 0.05f, 1000.0f);

	XMMATRIX invVP = XMMatrixInverse(nullptr, XMMatrixMultiply(view, proj));

	float ndcx = (2.0f * mx / W) - 1.0f;
	float ndcy = 1.0f - (2.0f * my / H);
	XMVECTOR pNear = XMVector3TransformCoord(XMVectorSet(ndcx, ndcy, 0.0f, 1.0f), invVP);
	XMVECTOR pFar = XMVector3TransformCoord(XMVectorSet(ndcx, ndcy, 1.0f, 1.0f), invVP);
	XMVECTOR dirv = XMVector3Normalize(XMVectorSubtract(pFar, pNear));

	XMStoreFloat3(&outOrigin, pNear);
	XMStoreFloat3(&outDir, dirv);
	return true;
}

bool SceneGame::ScreenRayToPlane(float planeY, DirectX::XMFLOAT3& outHit) const
{
	DirectX::XMFLOAT3 o, d;
	if (!ScreenRay(o, d)) return false;
	if (fabsf(d.y) < 1e-5f) return false;
	float t = (planeY - o.y) / d.y;
	if (t < 0.0f) return false;
	outHit = { o.x + d.x * t, planeY, o.z + d.z * t };
	return true;
}

void SceneGame::PaintTileAtMouse()
{
	if (activeTilePath.empty() || tileSize <= 0.0001f) return;

	DirectX::XMFLOAT3 hit;
	if (!ScreenRayToPlane(paintY, hit)) return;

	int cx = static_cast<int>(roundf(hit.x / tileSize));
	int cz = static_cast<int>(roundf(hit.z / tileSize));
	std::pair<int, int> key(cx, cz);
	if (tiles.find(key) != tiles.end()) return;

	std::shared_ptr<class Actor> a = ActorManager::Instance().Create();
	std::string name = std::filesystem::path(activeTilePath).stem().string();
	a->SetName(name.c_str());
	auto mc = a->AddComponent<ModelComponent>(activeTilePath.c_str());
	mc->SetShaderId(ShaderId::Toon);
	a->SetPosition({ cx * tileSize, paintY, cz * tileSize });

	TileEntry e; e.actor = a; e.path = activeTilePath;
	tiles[key] = e;
}

void SceneGame::EraseTileAtMouse()
{
	if (tileSize <= 0.0001f) return;
	DirectX::XMFLOAT3 hit;
	if (!ScreenRayToPlane(paintY, hit)) return;

	int cx = static_cast<int>(roundf(hit.x / tileSize));
	int cz = static_cast<int>(roundf(hit.z / tileSize));
	auto it = tiles.find(std::pair<int, int>(cx, cz));
	if (it == tiles.end()) return;
	if (it->second.actor) ActorManager::Instance().Remove(it->second.actor);
	tiles.erase(it);
}

void SceneGame::SaveTilemap()
{
	std::ofstream ofs("Tilemap.txt");
	if (!ofs) return;
	ofs << tileSize << " " << paintY << "\n";
	for (auto& kv : tiles)
		ofs << kv.first.first << " " << kv.first.second << " " << kv.second.path << "\n";
}

void SceneGame::LoadTilemap()
{
	std::ifstream ifs("Tilemap.txt");
	if (!ifs) return;

	ClearTilemap();
	ifs >> tileSize >> paintY;

	int cx, cz;
	while (ifs >> cx >> cz)
	{
		std::string path;
		std::getline(ifs, path);
		size_t s = path.find_first_not_of(" \t");
		if (s == std::string::npos) continue;
		path = path.substr(s);
		if (path.empty()) continue;

		std::shared_ptr<class Actor> a = ActorManager::Instance().Create();
		std::string name = std::filesystem::path(path).stem().string();
		a->SetName(name.c_str());
		auto mc = a->AddComponent<ModelComponent>(path.c_str());
		mc->SetShaderId(ShaderId::Toon);
		a->SetPosition({ cx * tileSize, paintY, cz * tileSize });

		TileEntry e; e.actor = a; e.path = path;
		tiles[std::pair<int, int>(cx, cz)] = e;
	}
}

void SceneGame::ClearTilemap()
{
	for (auto& kv : tiles)
		if (kv.second.actor) ActorManager::Instance().Remove(kv.second.actor);
	tiles.clear();
}
