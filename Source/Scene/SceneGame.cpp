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

namespace
{
	float Dot3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	float Dist3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
	{
		DirectX::XMFLOAT3 d = { a.x - b.x, a.y - b.y, a.z - b.z };
		return sqrtf(Dot3(d, d));
	}
}

SceneGame::SceneGame()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/diffuse_iem.dds", iblIrradiance.GetAddressOf());
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/specular_pmrem.dds", iblPrefiltered.GetAddressOf());
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/lut_ggx.dds", iblBrdfLut.GetAddressOf());
	Graphics::Instance().GetModelRenderer()->SetIBL(iblIrradiance.Get(), iblPrefiltered.Get(), iblBrdfLut.Get());

	GpuResourceUtils::LoadTexture(device, "Data/Sprite/bullet_hole.png", bulletHoleTex.GetAddressOf());

	camera.SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenWidth / screenHeight, 0.1f, 1000.0f);
	camera.SetLookAt({ 10.0f, 2.1f, 10.0f }, { 10.0f, 2.1f, 9.0f }, { 0, 1, 0 });
	cameraController.SyncCameraToController(camera);

	stageActor = ActorManager::Instance().Create();
	stageActor->SetName("Stage");
	auto stageModelComp = stageActor->AddComponent<ModelComponent>("Data/Model/Stage/RP_MAP_1.glb");

	CollisionMesh collisionMesh;
	Model* stageModel = stageModelComp->GetModel().get();

	for (const Model::Mesh& mesh : stageModel->GetMeshes())
	{
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		for (size_t i = 0; i < mesh.indices.size(); i += 3)
		{
			uint32_t a = mesh.indices.at(i + 0);
			uint32_t b = mesh.indices.at(i + 1);
			uint32_t c = mesh.indices.at(i + 2);

			DirectX::XMVECTOR A = DirectX::XMLoadFloat3(&mesh.vertices.at(a).position);
			DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&mesh.vertices.at(b).position);
			DirectX::XMVECTOR C = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mesh.vertices.at(c).position), WorldTransform);

			A = DirectX::XMVector3Transform(A, WorldTransform);
			B = DirectX::XMVector3Transform(B, WorldTransform);

			DirectX::XMVECTOR N = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(B, A), DirectX::XMVectorSubtract(C, A));
			if (DirectX::XMVector3Equal(N, DirectX::XMVectorZero())) continue;
			N = DirectX::XMVector3Normalize(N);

			CollisionMesh::Triangle& triangle = collisionMesh.triangles.emplace_back();
			DirectX::XMStoreFloat3(&triangle.positions[0], A);
			DirectX::XMStoreFloat3(&triangle.positions[1], B);
			DirectX::XMStoreFloat3(&triangle.positions[2], C);
			DirectX::XMStoreFloat3(&triangle.normal, N);
		}
	}
	collisionMesh.BuildSpatialGrid();
	stageCollision = collisionMesh;
	Actor::SetEditorGround(&stageCollision);

	playerActor = ActorManager::Instance().Create();
	playerActor->SetName("Player");
	playerActor->AddComponent<PlayerControllerComponent>(&camera, &cameraController, collisionMesh);
	playerActor->AddComponent<HealthComponent>(100.0f);

	viewModelActor = ActorManager::Instance().Create();
	viewModelActor->SetName("ViewModel");
	auto viewModelComp = viewModelActor->AddComponent<ModelComponent>("Data/Model/Weapon/Pistol.glb");
	viewModelComp->SetShaderId(ShaderId::Toon);
	viewModelActor->AddComponent<AnimatorComponent>();

	auto playerCtrl = playerActor->GetComponent<PlayerControllerComponent>();
	viewModelActor->AddComponent<ViewModelComponent>(&camera, playerCtrl.get());
	viewModelActor->AddComponent<CrosshairComponent>();

	helmetActor = ActorManager::Instance().Create();
	helmetActor->SetName("DamagedHelmet");
	auto helmetModelComp = helmetActor->AddComponent<ModelComponent>("Data/Model/Stage/DamagedHelmet.glb");
	helmetModelComp->SetShaderId(ShaderId::Pbr);
	helmetActor->SetPosition({ 10.0f, 2.1f, 7.0f });
	helmetActor->SetScale({ 1.0f, 1.0f, 1.0f });

	pbrSphereActor = ActorManager::Instance().Create();
	pbrSphereActor->SetName("PBR_Sphere");
	auto sphereModelComp = pbrSphereActor->AddComponent<ModelComponent>("Data/Model/Shape/Sphere.glb");
	sphereModelComp->SetShaderId(ShaderId::Pbr);
	pbrSphereActor->SetPosition({ 8.0f, 2.1f, 7.0f });
	pbrSphereActor->SetScale({ 1.0f, 1.0f, 1.0f });

	if (sphereModelComp->GetModel()) {
		auto& materials = sphereModelComp->GetModel()->GetMaterials();
		if (!materials.empty()) {
			materials[0].baseColor = { 1.0f, 0.8f, 0.2f, 1.0f };
			materials[0].metalness = 1.0f;
			materials[0].roughness = 0.1f;
		}
	}

	enemyActor = ActorManager::Instance().Create();
	enemyActor->SetName("Enemy");
	auto enemyModelComp = enemyActor->AddComponent<ModelComponent>("Data/Model/Character/Enemy3.glb");
	enemyModelComp->SetShaderId(ShaderId::Toon);
	enemyActor->AddComponent<AnimatorComponent>();
	auto enemyComp = enemyActor->AddComponent<EnemyComponent>(playerActor);
	enemyComp->SetStageCollision(collisionMesh);
	enemyActor->SetPosition({ 15.0f, 5.1f, 5.0f });
	enemyActor->SetScale({ 1.0f, 1.0f, 1.0f });

	BuildNavGrid();
	enemyComp->SetNavGrid(&navGrid);

	skybox.Initialize(device);

	editorCam.SyncFromCamera(camera);
	ScanAssets();
}

SceneGame::~SceneGame()
{
	ActorManager::Instance().Remove(stageActor);
	ActorManager::Instance().Remove(playerActor);
	ActorManager::Instance().Remove(viewModelActor);
	ActorManager::Instance().Remove(helmetActor);
	ActorManager::Instance().Remove(pbrSphereActor);
	ActorManager::Instance().Remove(enemyActor);
	for (std::shared_ptr<class Actor>& a : spawnedActors)
		if (a) ActorManager::Instance().Remove(a);
	for (auto& kv : tiles)
		if (kv.second.actor) ActorManager::Instance().Remove(kv.second.actor);
}

void SceneGame::Update(float elapsedTime)
{
	if (mode == EditorMode::Play)
	{
		ActorManager::Instance().Update(elapsedTime);
		ActorManager::Instance().UpdateTransform();
	}
	else // Edit
	{
		ActorManager::Instance().SyncPending();
		editorCam.Update(camera, elapsedTime);

		if (paintMode)
		{
			if (!ImGui::GetIO().WantCaptureMouse)
			{
				Mouse& m = Input::Instance().GetMouse();
				if (m.GetButtonDown() & Mouse::BTN_LEFT)
				{
					if (eraseMode) EraseTileAtMouse();
					else           PaintTileAtMouse();
				}
			}
		}
		else
		{
			UpdateGizmo();
		}

		ActorManager::Instance().UpdateTransform();
		ActorManager::Instance().EditorSync();
	}
}

// ---- Editor / Play mode ----
void SceneGame::StartPlay()
{
	SnapshotTransforms();
	mode = EditorMode::Play;
}

void SceneGame::StopPlay()
{
	mode = EditorMode::Edit;
	RestoreTransforms();
	editorCam.SyncFromCamera(camera);
}

void SceneGame::SnapshotTransforms()
{
	snapshots.clear();
	std::shared_ptr<class Actor> targets[] = { playerActor, enemyActor, helmetActor, pbrSphereActor };
	for (std::shared_ptr<class Actor>& a : targets)
		if (a) snapshots.push_back({ a, a->GetPosition(), a->GetRotation(), a->GetScale() });
	for (std::shared_ptr<class Actor>& a : spawnedActors)
		if (a) snapshots.push_back({ a, a->GetPosition(), a->GetRotation(), a->GetScale() });
}

void SceneGame::RestoreTransforms()
{
	for (ActorSnapshot& s : snapshots)
	{
		if (!s.actor) continue;
		s.actor->SetPosition(s.pos);
		s.actor->SetRotation(s.rot);
		s.actor->SetScale(s.scale);
	}
}

// ---- Content drawer ----
void SceneGame::ScanAssets()
{
	assetPaths.clear();
	namespace fs = std::filesystem;
	const char* root = "Data/Model";
	std::error_code ec;
	if (!fs::exists(root, ec)) return;

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
	std::sort(assetPaths.begin(), assetPaths.end());
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

		const DirectX::XMFLOAT3& p = a->GetPosition();
		const DirectX::XMFLOAT4& r = a->GetRotation();
		const DirectX::XMFLOAT3& s = a->GetScale();
		ofs << p.x << " " << p.y << " " << p.z << " "
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

	DirectX::XMFLOAT3 p, s;
	DirectX::XMFLOAT4 r;
	while (ifs >> p.x >> p.y >> p.z >> r.x >> r.y >> r.z >> r.w >> s.x >> s.y >> s.z)
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
		spawnedActors.push_back(a);
	}
}

void SceneGame::ClearScene()
{
	for (std::shared_ptr<class Actor>& a : spawnedActors)
		if (a) ActorManager::Instance().Remove(a);
	spawnedActors.clear();
}

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

// ---- Selection ----
std::shared_ptr<class Actor> SceneGame::PickActor(const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD)
{
	std::shared_ptr<class Actor> best;
	float bestT = FLT_MAX;

	for (const std::shared_ptr<class Actor>& a : ActorManager::Instance().GetActors())
	{
		if (!a) continue;
		const char* n = a->GetName();
		if (!strcmp(n, "Stage") || !strcmp(n, "Player") || !strcmp(n, "ViewModel")) continue;

		float dist;
		if (RayActorAABB(a, rO, rD, dist) && dist < bestT)
		{
			bestT = dist;
			best = a;
		}
	}
	return best;
}

void SceneGame::UpdateGizmo()
{
	Mouse& mouse = Input::Instance().GetMouse();
	bool rmb = (mouse.GetButton() & Mouse::BTN_RIGHT) != 0;

	// W/E/R switch gizmo operation (only when not flying / not typing).
	if (!rmb && !ImGui::GetIO().WantCaptureKeyboard)
	{
		if (GetAsyncKeyState('W') & 0x8000) gizmoMode = 0; // move
		if (GetAsyncKeyState('E') & 0x8000) gizmoMode = 1; // rotate
		if (GetAsyncKeyState('R') & 0x8000) gizmoMode = 2; // scale
	}

	// Left click selects an actor (ignored while an ImGuizmo handle is in use).
	bool lmbDown = (mouse.GetButtonDown() & Mouse::BTN_LEFT) != 0;
	if (lmbDown && !ImGui::GetIO().WantCaptureMouse && !ImGuizmo::IsUsing())
	{
		DirectX::XMFLOAT3 rO, rD;
		if (ScreenRay(rO, rD))
		{
			std::shared_ptr<class Actor> picked = PickActor(rO, rD);
			if (picked) ActorManager::Instance().SetSelected(picked); // keep selection on empty click
		}
	}
}

void SceneGame::DrawImGuizmo()
{
	if (mode != EditorMode::Edit) return;
	std::shared_ptr<class Actor> sel = ActorManager::Instance().GetSelectedActor();
	if (!sel) return;

	using namespace DirectX;

	// Build view / projection like Render().
	XMVECTOR R = XMLoadFloat3(&camera.GetRight());
	XMVECTOR U = XMLoadFloat3(&camera.GetUp());
	XMVECTOR F = XMLoadFloat3(&camera.GetFront());
	XMVECTOR E = XMLoadFloat3(&camera.GetEye());
	XMMATRIX camWorld;
	camWorld.r[0] = XMVectorSetW(R, 0.0f); camWorld.r[1] = XMVectorSetW(U, 0.0f);
	camWorld.r[2] = XMVectorSetW(F, 0.0f); camWorld.r[3] = XMVectorSetW(E, 1.0f);
	XMMATRIX viewM = XMMatrixInverse(nullptr, camWorld);

	float aspect = Graphics::Instance().GetScreenWidth() / Graphics::Instance().GetScreenHeight();
	XMMATRIX projM = XMMatrixPerspectiveFovLH(XMConvertToRadians(60), aspect, 0.05f, 1000.0f);

	XMFLOAT4X4 view, proj, model;
	XMStoreFloat4x4(&view, viewM);
	XMStoreFloat4x4(&proj, projM);
	model = sel->GetTransform();

	ImGuizmo::OPERATION op =
		(gizmoMode == 0) ? ImGuizmo::TRANSLATE :
		(gizmoMode == 1) ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
	ImGuizmo::MODE gmode = (gizmoMode == 2) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

	if (ImGuizmo::Manipulate(&view._11, &proj._11, op, gmode, &model._11))
	{
		XMVECTOR s, q, t;
		if (XMMatrixDecompose(&s, &q, &t, XMLoadFloat4x4(&model)))
		{
			XMFLOAT3 pos, scl; XMFLOAT4 rot;
			XMStoreFloat3(&pos, t);
			XMStoreFloat3(&scl, s);
			XMStoreFloat4(&rot, q);
			sel->SetPosition(pos);
			sel->SetRotation(rot);
			sel->SetScale(scl);
		}
	}
}

// ---- Navigation ----
void SceneGame::BuildNavGrid()
{
	DirectX::XMFLOAT3 seed = playerActor ? playerActor->GetPosition() : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	navGrid.Build(stageCollision, navCellSize, navAgentHeight, navMaxStep, seed);
}

void SceneGame::DrawNavMeshDebug(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
	if (!showNav || !navGrid.IsBuilt()) return;

	DirectX::XMFLOAT3 c = playerActor ? playerActor->GetPosition() : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	std::vector<DirectX::XMFLOAT3> reach, iso;
	navGrid.CollectDebugCells(c, navDebugRadius, reach, iso, 0.12f);

	PrimitiveRenderer* prim = Graphics::Instance().GetPrimitiveRenderer();
	const DirectX::XMFLOAT4 green = { 0.15f, 1.00f, 0.35f, 0.70f };
	const DirectX::XMFLOAT4 orange = { 1.00f, 0.60f, 0.10f, 0.55f };
	for (const DirectX::XMFLOAT3& v : reach) prim->AddVertex(v, green);
	for (const DirectX::XMFLOAT3& v : iso)   prim->AddVertex(v, orange);

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* rs = Graphics::Instance().GetRenderState();
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	prim->SetTexture(nullptr);
	prim->Render(dc, view, proj, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
}

void SceneGame::Render(float elapsedTime)
{
	RenderContext rc;
	rc.deviceContext = Graphics::Instance().GetDeviceContext();
	rc.renderState = Graphics::Instance().GetRenderState();
	rc.camera = &camera;

	skybox.Draw(camera);

	auto vmModelComp = viewModelActor->GetComponent<ModelComponent>();
	if (vmModelComp) vmModelComp->SetVisible(false);

	ActorManager::Instance().Render(rc);

	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&camera.GetRight());
	DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&camera.GetUp());
	DirectX::XMVECTOR F = DirectX::XMLoadFloat3(&camera.GetFront());
	DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&camera.GetEye());
	DirectX::XMMATRIX mCamWorld;
	mCamWorld.r[0] = DirectX::XMVectorSetW(R, 0.0f); mCamWorld.r[1] = DirectX::XMVectorSetW(U, 0.0f);
	mCamWorld.r[2] = DirectX::XMVectorSetW(F, 0.0f); mCamWorld.r[3] = DirectX::XMVectorSetW(E, 1.0f);
	DirectX::XMMATRIX mView = DirectX::XMMatrixInverse(nullptr, mCamWorld);

	float aspect = Graphics::Instance().GetScreenWidth() / Graphics::Instance().GetScreenHeight();
	DirectX::XMMATRIX mProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60), aspect, 0.05f, 1000.0f);

	DirectX::XMFLOAT4X4 view, proj;
	DirectX::XMStoreFloat4x4(&view, mView);
	DirectX::XMStoreFloat4x4(&proj, mProj);

	rc.deviceContext->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	rc.deviceContext->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
	Graphics::Instance().GetShapeRenderer()->Render(rc.deviceContext, view, proj);

	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
	Graphics::Instance().GetPrimitiveRenderer()->SetTexture(bulletHoleTex.Get());
	Graphics::Instance().GetPrimitiveRenderer()->Render(rc.deviceContext, view, proj, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Graphics::Instance().GetPrimitiveRenderer()->SetTexture(nullptr);

	DrawNavMeshDebug(view, proj);
	DrawSelectionOutline(view, proj);

	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;
	rc.deviceContext->OMGetRenderTargets(1, &rtv, &dsv);
	if (dsv) {
		rc.deviceContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		dsv->Release();
	}
	if (rtv) rtv->Release();

	if (vmModelComp && mode == EditorMode::Play) {
		vmModelComp->SetVisible(true);
		vmModelComp->Draw();
		Graphics::Instance().GetModelRenderer()->Render(rc);
	}
}

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
	static char assetFilter[128] = "";
	ImGui::PushItemWidth(240.0f);
	ImGui::InputTextWithHint("##assetfilter", "Search...", assetFilter, sizeof(assetFilter));
	ImGui::PopItemWidth();
	ImGui::TextDisabled(paintMode ? "Click a tile to set it ACTIVE, then paint in the viewport"
		: "Click an asset to place it in front of the camera");
	ImGui::Separator();
	ImGui::BeginChild("assetlist");
	for (const std::string& p : assetPaths)
	{
		std::string name = std::filesystem::path(p).filename().string();
		if (assetFilter[0] != '\0' && name.find(assetFilter) == std::string::npos) continue;
		bool isActive = (p == activeTilePath);
		if (ImGui::Selectable(name.c_str(), isActive))
		{
			if (paintMode) activeTilePath = p;
			else SpawnAsset(p);
		}
	}
	ImGui::EndChild();
	ImGui::End();

	// ---- ImGuizmo (move / rotate / scale the selected actor) ----
	DrawImGuizmo();
}

bool SceneGame::GetActorAABB(const std::shared_ptr<class Actor>& actor, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
	if (!actor) return false;
	auto mc = actor->GetComponent<ModelComponent>();
	if (!mc) return false;
	Model* model = mc->GetModel().get();
	if (!model) return false;

	auto it = aabbCache.find(model);
	if (it == aabbCache.end())
	{
		DirectX::XMFLOAT3 mn = { FLT_MAX, FLT_MAX, FLT_MAX };
		DirectX::XMFLOAT3 mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const Model::Mesh& mesh : model->GetMeshes())
		{
			DirectX::XMMATRIX nodeW = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
			for (const Model::Vertex& v : mesh.vertices)
			{
				DirectX::XMVECTOR p = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&v.position), nodeW);
				DirectX::XMFLOAT3 wp; DirectX::XMStoreFloat3(&wp, p);
				mn.x = (std::min)(mn.x, wp.x); mn.y = (std::min)(mn.y, wp.y); mn.z = (std::min)(mn.z, wp.z);
				mx.x = (std::max)(mx.x, wp.x); mx.y = (std::max)(mx.y, wp.y); mx.z = (std::max)(mx.z, wp.z);
			}
		}
		if (mn.x > mx.x) { mn = { -0.5f,-0.5f,-0.5f }; mx = { 0.5f,0.5f,0.5f }; }
		aabbCache[model] = { mn, mx };
		it = aabbCache.find(model);
	}
	outMin = it->second.first;
	outMax = it->second.second;
	return true;
}

bool SceneGame::RayActorAABB(const std::shared_ptr<class Actor>& actor, const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD, float& outDist)
{
	DirectX::XMFLOAT3 mn, mx;
	if (!GetActorAABB(actor, mn, mx)) return false;

	using namespace DirectX;
	XMMATRIX W = XMLoadFloat4x4(&actor->GetTransform());
	XMMATRIX invW = XMMatrixInverse(nullptr, W);
	XMFLOAT3 o, d;
	XMStoreFloat3(&o, XMVector3TransformCoord(XMLoadFloat3(&rO), invW));
	XMStoreFloat3(&d, XMVector3TransformNormal(XMLoadFloat3(&rD), invW));

	float tmin = -FLT_MAX, tmax = FLT_MAX;
	for (int i = 0; i < 3; ++i)
	{
		float O = (&o.x)[i], D = (&d.x)[i], Mn = (&mn.x)[i], Mx = (&mx.x)[i];
		if (fabsf(D) < 1e-8f) { if (O < Mn || O > Mx) return false; }
		else
		{
			float t1 = (Mn - O) / D, t2 = (Mx - O) / D;
			if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
		}
	}
	if (tmax < tmin || tmax < 0.0f) return false;

	float t = (tmin >= 0.0f) ? tmin : tmax;
	XMFLOAT3 localHit = { o.x + d.x * t, o.y + d.y * t, o.z + d.z * t };
	XMFLOAT3 worldHit;
	XMStoreFloat3(&worldHit, XMVector3TransformCoord(XMLoadFloat3(&localHit), W));
	outDist = Dist3(worldHit, rO);
	return true;
}

void SceneGame::DrawSelectionOutline(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
	if (mode != EditorMode::Edit) return;
	std::shared_ptr<class Actor> sel = ActorManager::Instance().GetSelectedActor();
	if (!sel) return;

	DirectX::XMFLOAT3 mn, mx;
	if (!GetActorAABB(sel, mn, mx)) return;

	using namespace DirectX;
	XMMATRIX W = XMLoadFloat4x4(&sel->GetTransform());
	XMFLOAT3 pts[8] = {
		{ mn.x,mn.y,mn.z }, { mx.x,mn.y,mn.z }, { mx.x,mn.y,mx.z }, { mn.x,mn.y,mx.z },
		{ mn.x,mx.y,mn.z }, { mx.x,mx.y,mn.z }, { mx.x,mx.y,mx.z }, { mn.x,mx.y,mx.z }
	};
	XMFLOAT3 c[8];
	for (int i = 0; i < 8; ++i)
		XMStoreFloat3(&c[i], XMVector3TransformCoord(XMLoadFloat3(&pts[i]), W));

	const int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
	};

	PrimitiveRenderer* prim = Graphics::Instance().GetPrimitiveRenderer();
	const XMFLOAT4 col = { 1.0f, 0.6f, 0.0f, 1.0f };
	for (int e = 0; e < 12; ++e)
	{
		prim->AddVertex(c[edges[e][0]], col);
		prim->AddVertex(c[edges[e][1]], col);
	}

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* rs = Graphics::Instance().GetRenderState();
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	prim->SetTexture(nullptr);
	prim->Render(dc, view, proj, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
}