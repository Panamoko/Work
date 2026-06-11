#pragma once

#include <vector>
#include <DirectXMath.h>
#include "GameObject/Component.h"
#include "GameObject/Components/ModelComponent.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"

// ステージの当たり判定情報
struct CollisionMesh
{
	struct Triangle
	{
		DirectX::XMFLOAT3	positions[3];
		DirectX::XMFLOAT3	normal;
	};
	std::vector<Triangle>	triangles;
};

// プレイヤーの移動、アニメーション制御、物理演算を担当するコンポーネント
class PlayerControllerComponent : public Component
{
public:
	// コンストラクタで、操作対象のカメラと、ステージの当たり判定（CollisionMesh）を受け取る
	PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh);
	~PlayerControllerComponent() override = default;

	const char* GetName() const override { return "PlayerController"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

private:
	bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);
	void MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical);

private:
	Camera* camera = nullptr;
	FpsCameraController* cameraController = nullptr;
	CollisionMesh						collisionMesh;

	DirectX::XMFLOAT3					position = { 10.0f, 0.5f, 10.0f }; // 初期位置
	float								radius = 0.5f;
	float								skinWidth = 0.01f;
	float								stepOffset = 0.1f;
	float								slopeLimit = 45.0f;

	std::vector<Model::NodePose>		nodePoses;
	int									animationIndex = -1;
	float								animationSeconds = 0.0f;
	float								oldAnimationSeconds = 0.0f;
	float								characterYaw = 0.0f;

	bool								useRootMotion = true;
	bool								usePhysics = true;
	float								manualMoveSpeed = 3.0f;

	char								rootNodeName[64] = "pelvis";
	char								animIdle[64] = "IDLE";
	char								animWalkF[64] = "WALK";
	char								animWalkB[64] = "BACK";
	char								animWalkL[64] = "LEFT";
	char								animWalkR[64] = "RIGHT";

	float								rootMotionScale = 1.0f;
	float								animSpeed = 1.0f;

	// デバッグモニター用
	DirectX::XMFLOAT3					debugRootMotion = { 0, 0, 0 };
	DirectX::XMFLOAT3					debugManualMotion = { 0, 0, 0 };
	int									debugAnimIndex = -1;
};