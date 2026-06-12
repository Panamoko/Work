#include "Graphics/Graphics.h"
#include "Scene/SceneGame.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h" // コンポーネントをインクルード
#include "GameObject/Components/AnimatorComponent.h" // ★追加：アニメーターをインクルード

SceneGame::SceneGame()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// =======================================================
	// カメラの初期セットアップ
	// =======================================================
	camera.SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenWidth / screenHeight, 0.1f, 1000.0f);
	camera.SetLookAt({ 10.0f, 2.1f, 10.0f }, { 10.0f, 2.1f, 9.0f }, { 0, 1, 0 });
	cameraController.SyncCameraToController(camera);

	// =======================================================
	// ステージアクターの生成と衝突メッシュ(CollisionMesh)の構築
	// =======================================================
	stageActor = ActorManager::Instance().Create();
	stageActor->SetName("Stage");
	auto stageModelComp = stageActor->AddComponent<ModelComponent>("Data/Model/Stage/Stage.glb");

	CollisionMesh collisionMesh;
	Model* stageModel = stageModelComp->GetModel().get();

	// ステージモデルの全メッシュから三角形ポリゴンを抽出して物理判定用のリストを作る
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

	playerActor = ActorManager::Instance().Create();
	playerActor->SetName("Player");
	playerActor->AddComponent<ModelComponent>("Data/Model/Mr.Incredible/Player.glb");

	// ★追加忘れ注意！：アニメーターコンポーネント
	playerActor->AddComponent<AnimatorComponent>();

	// =======================================================
	// ★ここを追加：武器アクターを作り、PlayerControllerに渡す！
	// =======================================================
	std::shared_ptr<Actor> weaponActor = ActorManager::Instance().Create();
	weaponActor->SetName("Weapon");
	// ※ライフルのファイルパスはあなたの環境に合わせてください
	weaponActor->AddComponent<ModelComponent>("Data/Model/Weapon/Rifle.glb");

	// ★ここを修正：第4引数として `weaponActor.get()` を渡す！
	playerActor->AddComponent<PlayerControllerComponent>(&camera, &cameraController, collisionMesh, weaponActor.get());
}

SceneGame::~SceneGame()
{
	// シーン終了時、確実にアクターを削除してメモリを解放する
	ActorManager::Instance().Remove(playerActor);
	ActorManager::Instance().Remove(stageActor);
}

void SceneGame::Update(float elapsedTime)
{
	// 全てのアクターとコンポーネントのロジックを一括更新
	ActorManager::Instance().Update(elapsedTime);

	// 全てのアクターのワールド行列を一括更新
	ActorManager::Instance().UpdateTransform();
}

void SceneGame::Render(float elapsedTime)
{
	RenderContext rc;
	rc.deviceContext = Graphics::Instance().GetDeviceContext();
	rc.renderState = Graphics::Instance().GetRenderState();
	rc.camera = &camera;

	// 全てのアクターの描画を一括処理
	ActorManager::Instance().Render(rc);
}

void SceneGame::DrawGUI()
{
	// UIの描画も各コンポーネントの OnGUI() が自動で呼ばれるため、シーン側では何もしない
}