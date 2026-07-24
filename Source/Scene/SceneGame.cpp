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
	// Player needs an Animator to drive the full-body Voxel model.
	// Added before the model so it updates first (pose -> model same frame).
	playerActor->AddComponent<AnimatorComponent>();

	// Full-body player model (Voxel). Swap in Settings > Player.
	auto playerModelComp = playerActor->AddComponent<ModelComponent>("Data/Model/GLB format/character-a.glb");
	playerModelComp->SetShaderId(ShaderId::Toon);

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

		// F5 toggles FPS <-> TPS (Minecraft-style).
		bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
		if (f5 && !prevF5) cameraView ^= 1;
		prevF5 = f5;

		ApplyCameraView();
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
	cameraView = 0; // start in FPS
	prevF5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
	ApplyCameraView();
}


void SceneGame::StopPlay()
{
	mode = EditorMode::Edit;
	RestoreTransforms();

	// Restore the head mesh when returning to Edit.
	if (playerActor)
		if (auto m = playerActor->GetComponent<ModelComponent>())
			m->SetMeshVisible("Head", true);

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

	// The weapon is now a normal bone-attached world actor: show it in Play,
	// hide it in Edit (it is not positioned there).
	auto vmModelComp = viewModelActor->GetComponent<ModelComponent>();
	if (vmModelComp) vmModelComp->SetVisible(mode == EditorMode::Play);

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
}


void SceneGame::ApplyCameraView()
{
	// FPS = hide only the head MESH (keep body/arms drawn). TPS = show everything.
	// Uses per-mesh visibility, so it generalizes to Helmet/Armor later.
	if (playerActor)
	{
		if (auto m = playerActor->GetComponent<ModelComponent>())
		{
			m->SetVisible(true);                        // body always drawn
			m->SetMeshVisible("Head", cameraView == 0); // ← GLBの頭メッシュのノード名に合わせる
		}
	}

	// TPS: pull the camera back along its view direction, keeping the same look.
	if (cameraView == 1 && playerActor)
	{
		DirectX::XMFLOAT3 f = camera.GetFront();
		DirectX::XMFLOAT3 p = playerActor->GetPosition();
		DirectX::XMFLOAT3 focus = { p.x, p.y + tpsHeight, p.z };
		DirectX::XMFLOAT3 eye = {
			focus.x - f.x * tpsDistance,
			focus.y - f.y * tpsDistance,
			focus.z - f.z * tpsDistance
		};
		camera.SetLookAt(eye, focus, { 0.0f, 1.0f, 0.0f });
	}
}