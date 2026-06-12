#pragma once

#include <vector>
#include <DirectXMath.h>
#include "GameObject/Component.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"

struct CollisionMesh
{
	struct Triangle { DirectX::XMFLOAT3 positions[3]; DirectX::XMFLOAT3 normal; };
	std::vector<Triangle> triangles;
};

enum class PlayerState { Idle, Walk, Jump, Fall };

class PlayerControllerComponent : public Component
{
public:
	PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh, class Actor* weaponActor = nullptr);
	~PlayerControllerComponent() override = default;

	const char* GetName() const override { return "PlayerController"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

private:
	bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);
	void MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical);
	bool CheckGrounded();

private:
	ModelComponent* cachedModelComponent = nullptr;
	AnimatorComponent* animator = nullptr;

	class Actor* weaponActor = nullptr;

	Camera* camera = nullptr;
	FpsCameraController* cameraController = nullptr;
	CollisionMesh						collisionMesh;

	DirectX::XMFLOAT3					position = { 10.0f, 0.5f, 10.0f };
	float								radius = 0.5f;
	float								skinWidth = 0.01f;
	float								stepOffset = 0.1f;
	float								slopeLimit = 45.0f;

	PlayerState							currentState = PlayerState::Idle;
	bool								isGrounded = false;
	float								velocityY = 0.0f;
	float								gravity = 15.0f;
	float								jumpPower = 8.0f;

	float								characterYaw = 0.0f;
	float								cameraPitch = 0.0f;
	float								mouseSensitivity = 0.002f;

	bool								usePhysics = true;
	float								manualMoveSpeed = 5.0f;

	char								animIdle[64] = "IDLE";
	char								animWalkF[64] = "WALK";
	char								animWalkB[64] = "BACK";
	char								animWalkL[64] = "LEFT";
	char								animWalkR[64] = "RIGHT";
	char								animJump[64] = "JUMP";
	char								animFall[64] = "FALL";

	float								crossFadeTime = 0.15f;
	float								fallClampSeconds = 1.0f;
	float								animSpeed = 1.0f;

	int									currentAnimZIndex = -1;
	int									currentAnimXIndex = -1;

	// ===============================================
	// ★追加：最強のデバッグ用変数群
	// ===============================================
	bool								isCursorLocked = false; // FPS操作中かどうか
	bool								prevTabKey = false;     // TABキーの1フレーム前の状態
	bool								cursorVisibilityState = true; // 現在のカーソル表示状態

	DirectX::XMFLOAT3					cameraOffsetPos = { 0.0f, 1.65f, 0.0f }; // カメラの高さ調整用
	char								handBoneName[64] = "hand_r"; // GUIからボーン名を変えられるように

	DirectX::XMFLOAT3                   weaponOffsetPos = { 0.25f, -0.25f, 0.4f };
	DirectX::XMFLOAT3                   weaponOffsetRot = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3                   weaponOffsetScale = { 1.0f, 1.0f, 1.0f };

	DirectX::XMFLOAT3					debugManualMotion = { 0, 0, 0 };
};