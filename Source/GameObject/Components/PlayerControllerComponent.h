#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <DirectXMath.h>
#include "GameObject/Component.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"
#include "Physics/CollisionMesh.h"

enum class PlayerState { Idle, Walk, Run, Jump, Fall };

class PlayerControllerComponent : public Component {
public:
	PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh);
	~PlayerControllerComponent() override = default;

	const char* GetName() const override { return "PlayerController"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	void SaveSettings();
	void LoadSettings();

	PlayerState GetCurrentState() const { return currentState; }
	bool IsGrounded() const { return isGrounded; }

	// Weapon attach: let the ViewModel read the player body's bones.
	ModelComponent* GetModelComponent() const { return cachedModelComponent; }

	bool RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);
	void SetMoveSpeeds(float walk, float run) { manualMoveSpeed = walk; runMoveSpeed = run; }
private:

	bool CapsuleCast(const DirectX::XMFLOAT3& originFoot, const DirectX::XMFLOAT3& originHead, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);

	void MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical);
	bool CheckGrounded();

	void UpdateAnimation(Model* characterModel, bool stateChanged, const DirectX::XMFLOAT3& manualVec);
	void UpdateCameraFollow(DirectX::FXMVECTOR currentVelocity, float elapsedTime);

	ModelComponent* cachedModelComponent = nullptr;
	AnimatorComponent* animator = nullptr;
	Camera* camera = nullptr;
	FpsCameraController* cameraController = nullptr;
	CollisionMesh collisionMesh;

	DirectX::XMFLOAT3 position = { 10.0f, 10.0f, 10.0f };
	float radius = 0.2f;
	float skinWidth = 0.01f;
	float stepOffset = 0.1f;
	float slopeLimit = 45.0f;

	PlayerState currentState = PlayerState::Idle;
	bool isGrounded = false;

	// 重力・ジャンプ処理
	float velocityY = 0.0f;
	float gravity = 35.0f;
	float jumpPower = 13.0f;

	float characterYaw = 0.0f;
	float cameraPitch = 0.0f;
	float mouseSensitivity = 0.002f;

	bool usePhysics = true;

	// 移動速度
	float manualMoveSpeed = 4.0f;
	float runMoveSpeed = 8.0f;
	float currentFov = 60.0f;

	DirectX::XMFLOAT3 currentVelocity = { 0.0f, 0.0f, 0.0f };
	float acceleration = 15.0f;
	float deceleration = 20.0f;
	float airControl = 0.2f;

	// Voxel character clip names (character-*.glb): idle / walk / sprint ...
	char animIdle[64] = "idle";
	char animWalkF[64] = "walk";
	char animWalkB[64] = "walk";
	char animWalkL[64] = "walk";
	char animWalkR[64] = "walk";
	char animRun[64] = "sprint";
	char animJump[64] = "jump";
	char animFall[64] = "fall";

	float crossFadeTime = 0.15f;
	float fallClampSeconds = 1.0f;
	float animSpeed = 1.0f;

	int currentAnimZIndex = -1;
	int currentAnimXIndex = -1;

	bool isCursorLocked = false;
	bool prevTabKey = false;
	bool prevSpaceKey = false;
	bool cursorVisibilityState = true;

	DirectX::XMFLOAT3 debugManualMotion = { 0, 0, 0 };
	float playerScale = 1.0f;

	DirectX::XMFLOAT3 cameraOffsetPos = { 0.0f, 1.5f, 0.0f };

	bool isFullscreen = false;
	bool prevF11Key = false;
	RECT savedWindowRect = { 0, 0, 1280, 720 };
};