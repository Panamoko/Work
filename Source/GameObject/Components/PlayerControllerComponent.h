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

struct CollisionMesh {
	struct Triangle { DirectX::XMFLOAT3 positions[3]; DirectX::XMFLOAT3 normal; };
	std::vector<Triangle> triangles;

	float cellSize = 3.0f;
	std::unordered_map<unsigned long long, std::vector<size_t>> gridCells;

	unsigned long long ComputeHash(int x, int y, int z) const {
		unsigned long long hx = static_cast<unsigned long long>(x & 0x1FFFFF);
		unsigned long long hy = static_cast<unsigned long long>(y & 0x1FFFFF);
		unsigned long long hz = static_cast<unsigned long long>(z & 0x1FFFFF);
		return (hx << 42) | (hy << 21) | hz;
	}

	void BuildSpatialGrid() {
		gridCells.clear();
		for (size_t i = 0; i < triangles.size(); ++i) {
			const auto& tri = triangles[i];
			float minX = (std::min)({ tri.positions[0].x, tri.positions[1].x, tri.positions[2].x });
			float minY = (std::min)({ tri.positions[0].y, tri.positions[1].y, tri.positions[2].y });
			float minZ = (std::min)({ tri.positions[0].z, tri.positions[1].z, tri.positions[2].z });
			float maxX = (std::max)({ tri.positions[0].x, tri.positions[1].x, tri.positions[2].x });
			float maxY = (std::max)({ tri.positions[0].y, tri.positions[1].y, tri.positions[2].y });
			float maxZ = (std::max)({ tri.positions[0].z, tri.positions[1].z, tri.positions[2].z });

			int startX = static_cast<int>(std::floor(minX / cellSize));
			int startY = static_cast<int>(std::floor(minY / cellSize));
			int startZ = static_cast<int>(std::floor(minZ / cellSize));
			int endX = static_cast<int>(std::floor(maxX / cellSize));
			int endY = static_cast<int>(std::floor(maxY / cellSize));
			int endZ = static_cast<int>(std::floor(maxZ / cellSize));

			for (int x = startX; x <= endX; ++x) {
				for (int y = startY; y <= endY; ++y) {
					for (int z = startZ; z <= endZ; ++z) {
						gridCells[ComputeHash(x, y, z)].push_back(i);
					}
				}
			}
		}
	}
};

enum class PlayerState { Idle, Walk, Jump, Fall };

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

	// =======================================================
	// ★追加：Hitscan（即着弾）射撃用のRaycast判定
	// =======================================================
	bool RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);

private:
	bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);
	void MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical);
	bool CheckGrounded();

	ModelComponent* cachedModelComponent = nullptr;
	AnimatorComponent* animator = nullptr;
	Camera* camera = nullptr;
	FpsCameraController* cameraController = nullptr;
	CollisionMesh collisionMesh;

	DirectX::XMFLOAT3 position = { 10.0f, 0.5f, 10.0f };
	float radius = 0.2f;
	float skinWidth = 0.01f;
	float stepOffset = 0.1f;
	float slopeLimit = 45.0f;

	PlayerState currentState = PlayerState::Idle;
	bool isGrounded = false;
	float velocityY = 0.0f;
	float gravity = 15.0f;
	float jumpPower = 8.0f;

	float characterYaw = 0.0f;
	float cameraPitch = 0.0f;
	float mouseSensitivity = 0.002f;

	bool usePhysics = true;
	float manualMoveSpeed = 5.0f;

	char animIdle[64] = "IDLE";
	char animWalkF[64] = "WALK";
	char animWalkB[64] = "BACK";
	char animWalkL[64] = "LEFT";
	char animWalkR[64] = "RIGHT";
	char animJump[64] = "JUMP";
	char animFall[64] = "FALL";

	float crossFadeTime = 0.15f;
	float fallClampSeconds = 1.0f;
	float animSpeed = 1.0f;

	int currentAnimZIndex = -1;
	int currentAnimXIndex = -1;

	bool isCursorLocked = false;
	bool prevTabKey = false;
	bool cursorVisibilityState = true;

	DirectX::XMFLOAT3 debugManualMotion = { 0, 0, 0 };
	float playerScale = 1.0f;

	DirectX::XMFLOAT3 cameraOffsetPos = { 0.0f, 1.5f, 0.0f };

	bool isFullscreen = false;
	bool prevF11Key = false;
	RECT savedWindowRect = { 0, 0, 1280, 720 };
};