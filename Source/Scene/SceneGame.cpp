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
#include <string> // 文字列処理用

SceneGame::SceneGame()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// IBLテクスチャのロードと、ModelRendererへのセット
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/diffuse_iem.dds", iblIrradiance.GetAddressOf());
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/specular_pmrem.dds", iblPrefiltered.GetAddressOf());
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/lut_ggx.dds", iblBrdfLut.GetAddressOf());

	Graphics::Instance().GetModelRenderer()->SetIBL(iblIrradiance.Get(), iblPrefiltered.Get(), iblBrdfLut.Get());

	// カメラの初期セットアップ
	camera.SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenWidth / screenHeight, 0.1f, 1000.0f);
	camera.SetLookAt({ 10.0f, 2.1f, 10.0f }, { 10.0f, 2.1f, 9.0f }, { 0, 1, 0 });
	cameraController.SyncCameraToController(camera);

	// ステージアクターの生成
	stageActor = ActorManager::Instance().Create();
	stageActor->SetName("Stage");
	auto stageModelComp = stageActor->AddComponent<ModelComponent>("Data/Model/Stage/Stage.glb");

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
			DirectX::XMVECTOR C = DirectX::XMLoadFloat3(&mesh.vertices.at(c).position);

			A = DirectX::XMVector3Transform(A, WorldTransform);
			B = DirectX::XMVector3Transform(B, WorldTransform);
			C = DirectX::XMVector3Transform(C, WorldTransform);

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

	// プレイヤーアクターの生成
	playerActor = ActorManager::Instance().Create();
	playerActor->SetName("Player");
	playerActor->AddComponent<PlayerControllerComponent>(&camera, &cameraController, collisionMesh);

	// FPS視点専用の「腕＋銃」アクター（ViewModel）
	viewModelActor = ActorManager::Instance().Create();
	viewModelActor->SetName("ViewModel");
	auto viewModelComp = viewModelActor->AddComponent<ModelComponent>("Data/Model/Weapon/Pistol.glb");
	viewModelComp->SetShaderId(ShaderId::Toon);
	viewModelActor->AddComponent<AnimatorComponent>();

	auto playerCtrl = playerActor->GetComponent<PlayerControllerComponent>();
	viewModelActor->AddComponent<ViewModelComponent>(&camera, playerCtrl.get());
	viewModelActor->AddComponent<CrosshairComponent>();

	// PBR確認用アクター群
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

	ActorManager::Instance().Render(rc);

	// 蓄積された3Dエフェクト（弾道・火花・弾痕）を描画する
	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&camera.GetRight());
	DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&camera.GetUp());
	DirectX::XMVECTOR F = DirectX::XMLoadFloat3(&camera.GetFront());
	DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&camera.GetEye());
	DirectX::XMMATRIX mCamWorld;
	mCamWorld.r[0] = DirectX::XMVectorSetW(R, 0.0f); mCamWorld.r[1] = DirectX::XMVectorSetW(U, 0.0f);
	mCamWorld.r[2] = DirectX::XMVectorSetW(F, 0.0f); mCamWorld.r[3] = DirectX::XMVectorSetW(E, 1.0f);
	DirectX::XMMATRIX mView = DirectX::XMMatrixInverse(nullptr, mCamWorld);

	float aspect = Graphics::Instance().GetScreenWidth() / Graphics::Instance().GetScreenHeight();
	DirectX::XMMATRIX mProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60), aspect, 0.35f, 1000.0f);

	DirectX::XMFLOAT4X4 view, proj;
	DirectX::XMStoreFloat4x4(&view, mView);
	DirectX::XMStoreFloat4x4(&proj, mProj);

	Graphics::Instance().GetShapeRenderer()->Render(rc.deviceContext, view, proj);
}

void SceneGame::DrawGUI()
{
	// =======================================================
	// ★大掃除完了！UnityライクなエディタUIの構築
	// =======================================================

	// 【 1. HIERARCHY (ヒエラルキー) 】
	// シーン内に存在するアクターを一覧表示し、クリックで選択状態にする
	ImGui::Begin("Hierarchy");
	ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"▼ シーン内のアクター");
	ImGui::Separator();

	if (ImGui::Selectable("Stage", selectedActor == stageActor)) selectedActor = stageActor;
	if (ImGui::Selectable("Player", selectedActor == playerActor)) selectedActor = playerActor;
	if (ImGui::Selectable("ViewModel (FPS Arms)", selectedActor == viewModelActor)) selectedActor = viewModelActor;
	if (ImGui::Selectable("DamagedHelmet", selectedActor == helmetActor)) selectedActor = helmetActor;
	if (ImGui::Selectable("PBR_Sphere", selectedActor == pbrSphereActor)) selectedActor = pbrSphereActor;

	// グローバル設定はHierarchyの下部に置いておく
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ ライティング設定");
	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
	ImGui::Checkbox(u8"IBL(環境光反射) を有効にする", &renderer->enableIBL);
	ImGui::End();


	// 【 2. INSPECTOR (インスペクター) 】
	// 選択されたアクターが持っているコンポーネントのUIを自動的に列挙して描画する
	ImGui::Begin("Inspector");
	if (selectedActor)
	{
		// C++のクラッシュを防ぐため、名前は安全に判別して表示
		std::string actorName = "Unknown";
		if (selectedActor == stageActor) actorName = "Stage";
		else if (selectedActor == playerActor) actorName = "Player";
		else if (selectedActor == viewModelActor) actorName = "ViewModel";
		else if (selectedActor == helmetActor) actorName = "DamagedHelmet";
		else if (selectedActor == pbrSphereActor) actorName = "PBR_Sphere";

		ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"■ 選択中: %s", actorName.c_str());
		ImGui::Separator();

		// 以下、アクターが持っているコンポーネントを片っ端から確認し、
		// 存在していればそのコンポーネント専用のGUI（OnGUI）を呼び出す

		auto modelComp = selectedActor->GetComponent<ModelComponent>();
		if (modelComp) {
			modelComp->OnGUI();
			ImGui::Separator();
		}

		auto playerCtrl = selectedActor->GetComponent<PlayerControllerComponent>();
		if (playerCtrl) {
			playerCtrl->OnGUI();
			ImGui::Separator();
		}

		auto viewModel = selectedActor->GetComponent<ViewModelComponent>();
		if (viewModel) {
			viewModel->OnGUI();
			ImGui::Separator();
		}

		auto crosshair = selectedActor->GetComponent<CrosshairComponent>();
		if (crosshair) {
			crosshair->OnGUI();
			ImGui::Separator();
		}

		auto animator = selectedActor->GetComponent<AnimatorComponent>();
		if (animator) {
			animator->OnGUI();
			ImGui::Separator();
		}
	}
	else
	{
		// 何も選択されていない場合
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), u8"アクターが選択されていません");
		ImGui::Text(u8"Hierarchyパネルからアクターを選択してください。");
	}
	ImGui::End();
}