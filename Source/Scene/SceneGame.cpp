#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h" 
#include "Scene/SceneGame.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h" 
#include "GameObject/Components/AnimatorComponent.h" 
#include "GameObject/Components/ViewModelComponent.h" 
#include "GameObject/Components/CrosshairComponent.h"
#include <imgui.h>
#include <string> 

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

	playerActor = ActorManager::Instance().Create();
	playerActor->SetName("Player");
	playerActor->AddComponent<PlayerControllerComponent>(&camera, &cameraController, collisionMesh);

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
}

SceneGame::~SceneGame()
{
	ActorManager::Instance().Remove(stageActor);
	ActorManager::Instance().Remove(playerActor);
	ActorManager::Instance().Remove(viewModelActor);
	ActorManager::Instance().Remove(helmetActor);
	ActorManager::Instance().Remove(pbrSphereActor);
}

void SceneGame::Update(float elapsedTime)
{
	ActorManager::Instance().Update(elapsedTime);
	ActorManager::Instance().UpdateTransform();
}

void SceneGame::Render(float elapsedTime)
{
	RenderContext rc;
	rc.deviceContext = Graphics::Instance().GetDeviceContext();
	rc.renderState = Graphics::Instance().GetRenderState();
	rc.camera = &camera;

	// =======================================================
	// ★プロのFPSハック Step 1：ワールド描画の前にViewModelを一旦隠す
	// =======================================================
	auto vmModelComp = viewModelActor->GetComponent<ModelComponent>();
	if (vmModelComp) vmModelComp->SetVisible(false);

	// 背景や敵など、ワールド全体を描画
	ActorManager::Instance().Render(rc);

	// エフェクト（弾道・火花・弾痕）を描画
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
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	// =======================================================
	// ★プロのFPSハック Step 2：深度バッファ(Zバッファ)を強制クリア
	// DirectX11のコア機能にアクセスして、画面の奥行き情報をリセットする
	// =======================================================
	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;
	rc.deviceContext->OMGetRenderTargets(1, &rtv, &dsv);
	if (dsv) {
		rc.deviceContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		dsv->Release();
	}
	if (rtv) rtv->Release();

	// =======================================================
	// ★プロのFPSハック Step 3：ViewModelだけを「最前面」に描画
	// =======================================================
	if (vmModelComp) {
		vmModelComp->SetVisible(true);
		vmModelComp->Draw(); // ModelRendererのキューに登録
		Graphics::Instance().GetModelRenderer()->Render(rc); // キューをフラッシュして単独で描画
	}
}

void SceneGame::DrawGUI()
{
	ImGui::SetNextWindowPos(ImVec2(20, 460), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(280, 70), ImGuiCond_FirstUseEver);

	ImGui::Begin(u8"Render Settings");
	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
	ImGui::Checkbox(u8"IBL（環境光反射）を有効にする", &renderer->enableIBL);
	ImGui::End();
}