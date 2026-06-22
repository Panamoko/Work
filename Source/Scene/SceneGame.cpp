#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h" // ★追加：テクスチャ読み込み用
#include "Scene/SceneGame.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h" 
#include "GameObject/Components/AnimatorComponent.h" 
#include "GameObject/Components/ViewModelComponent.h" 
#include "GameObject/Components/CrosshairComponent.h"
#include <imgui.h>

SceneGame::SceneGame()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// =======================================================
	// ★追加：IBLテクスチャのロードと、ModelRendererへのセット
	// =======================================================
	// 1. 拡散環境光（Irradiance）
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/diffuse_iem.dds", iblIrradiance.GetAddressOf());

	// 2. 鏡面環境光（Prefiltered）
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/specular_pmrem.dds", iblPrefiltered.GetAddressOf());

	// 3. BRDFルックアップテーブル
	GpuResourceUtils::LoadTexture(device, "Data/Sprite/IBL/lut_ggx.dds", iblBrdfLut.GetAddressOf());

	// ロードしたテクスチャを、完璧に準備されていた ModelRenderer に渡す！
	Graphics::Instance().GetModelRenderer()->SetIBL(iblIrradiance.Get(), iblPrefiltered.Get(), iblBrdfLut.Get());
	// =======================================================


	// =======================================================
	// カメラの初期セットアップ
	// =======================================================
	camera.SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenWidth / screenHeight, 0.1f, 1000.0f);
	camera.SetLookAt({ 10.0f, 2.1f, 10.0f }, { 10.0f, 2.1f, 9.0f }, { 0, 1, 0 });
	cameraController.SyncCameraToController(camera);

	// =======================================================
	// ステージアクターの生成
	// =======================================================
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
	collisionMesh.BuildSpatialGrid(); // 空間ハッシュグリッドの構築（Broad Phase）

	// =======================================================
	// プレイヤーアクターの生成（当たり判定・カメラ操作用）
	// =======================================================
	playerActor = ActorManager::Instance().Create();
	playerActor->SetName("Player");

	// 都築の修正：WeaponActorへの依存を完全に切り離し（nullptrを渡さず、メンバそのものを削除）
	playerActor->AddComponent<PlayerControllerComponent>(&camera, &cameraController, collisionMesh);

	// =======================================================
	// ★新規：FPS視点専用の「腕＋銃」アクター（ViewModel）
	// =======================================================
	// 【重要】必ず playerActor の後で Create すること。
	// これにより Update() の順番が Player -> ViewModel となり、カメラ遅延（1フレームのズレ）を防げます。
	viewModelActor = ActorManager::Instance().Create();
	viewModelActor->SetName("ViewModel");

	// Sketchfab等で用意した「腕と銃」のモデルパスを指定してください
	viewModelActor->AddComponent<ModelComponent>("Data/Model/Weapon/Pistol.glb");
	viewModelActor->AddComponent<AnimatorComponent>();

	// 自動補正コンポーネントをアタッチし、追従すべきカメラを渡す
	auto playerCtrl = playerActor->GetComponent<PlayerControllerComponent>();
	viewModelActor->AddComponent<ViewModelComponent>(&camera, playerCtrl.get());

	// 修正：弾と連携しやすくするため、ViewModelのアクターにクロスヘアを持たせる
	viewModelActor->AddComponent<CrosshairComponent>();

	// =======================================================
	// ★PBR確認用：DamagedHelmet を配置
	// =======================================================
	helmetActor = ActorManager::Instance().Create();
	helmetActor->SetName("DamagedHelmet");
	auto helmetModelComp = helmetActor->AddComponent<ModelComponent>("Data/Model/Stage/DamagedHelmet.glb");
	helmetModelComp->SetShaderId(ShaderId::Pbr);             // PBRシェーダーで描画
	helmetActor->SetPosition({ 10.0f, 2.1f, 7.0f });        // 開始時のカメラ前方
	helmetActor->SetScale({ 1.0f, 1.0f, 1.0f });
	pbrSphereActor = ActorManager::Instance().Create();
	pbrSphereActor->SetName("PBR_Sphere");
	auto sphereModelComp = pbrSphereActor->AddComponent<ModelComponent>("Data/Model/Shape/Sphere.glb");
	sphereModelComp->SetShaderId(ShaderId::Pbr);
	pbrSphereActor->SetPosition({ 8.0f, 2.1f, 7.0f }); // ヘルメットの少し左に配置
	pbrSphereActor->SetScale({ 1.0f, 1.0f, 1.0f });

	// 球体の初期マテリアルを分かりやすく「金ピカの鏡」にしておく
	if (sphereModelComp->GetModel()) {
		auto& materials = sphereModelComp->GetModel()->GetMaterials();
		if (!materials.empty()) {
			materials[0].baseColor = { 1.0f, 0.8f, 0.2f, 1.0f }; // ゴールド
			materials[0].metalness = 1.0f;                       // 1.0=完全な金属
			materials[0].roughness = 0.1f;                       // 0.0=完全にツルツル
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

	// =======================================================
	// 蓄積された3Dエフェクト（弾道・火花・弾痕）を描画する
	// =======================================================
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

	// ShapeRendererを呼び出して、キューに溜まったエフェクトを一気に描画
	Graphics::Instance().GetShapeRenderer()->Render(rc.deviceContext, view, proj);
}

void SceneGame::DrawGUI()
{
	// Render Settings (global). Per-material editing now lives in each
	// object's Inspector (ModelComponent::OnGUI).
	ImGui::SetNextWindowPos(ImVec2(20, 460), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(280, 90), ImGuiCond_FirstUseEver);

	ImGui::Begin(u8"Render Settings");
	ModelRenderer* renderer = Graphics::Instance().GetModelRenderer();
	ImGui::Checkbox(u8"IBL (Image Based Lighting)", &renderer->enableIBL);
	ImGui::TextDisabled(u8"Material params are in each object's Inspector");
	ImGui::End();
}