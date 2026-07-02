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


	// 敵（アニメ入りキャラ：Player3.glb）
	enemyActor = ActorManager::Instance().Create();
	enemyActor->SetName("Enemy");
	auto enemyModelComp = enemyActor->AddComponent<ModelComponent>("Data/Model/Mr.Incredible/Player3.glb");
	enemyModelComp->SetShaderId(ShaderId::Toon);
	enemyActor->AddComponent<AnimatorComponent>();           // ★追加：クロスフェード用
	enemyActor->AddComponent<EnemyComponent>(playerActor);
	enemyActor->SetPosition({ 10.0f, 2.1f, 4.0f });
	enemyActor->SetScale({ 1.0f, 1.0f, 1.0f });

	skybox.Initialize(device);
}

SceneGame::~SceneGame()
{
	ActorManager::Instance().Remove(stageActor);
	ActorManager::Instance().Remove(playerActor);
	ActorManager::Instance().Remove(viewModelActor);
	ActorManager::Instance().Remove(helmetActor);
	ActorManager::Instance().Remove(pbrSphereActor);
	ActorManager::Instance().Remove(enemyActor);
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

	// 背景（スカイ）を最初に描く
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
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;
	rc.deviceContext->OMGetRenderTargets(1, &rtv, &dsv);
	if (dsv) {
		rc.deviceContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		dsv->Release();
	}
	if (rtv) rtv->Release();

	if (vmModelComp) {
		vmModelComp->SetVisible(true);
		vmModelComp->Draw();
		Graphics::Instance().GetModelRenderer()->Render(rc);
	}
}

void SceneGame::DrawGUI()
{
	ImGui::SetNextWindowPos(ImVec2(20, 340), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(340, 460), ImGuiCond_FirstUseEver);

	ImGui::Begin("Render Settings");

	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();

	// One-click "look" that sets sky + post + shadow together (unified art direction).
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

	if (ImGui::CollapsingHeader("Art Style (Presets)", ImGuiTreeNodeFlags_DefaultOpen))
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

	ImGui::End();
}