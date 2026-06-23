#pragma once

#include <vector>
#include <string>
#include <DirectXMath.h>
#include "GameObject/Component.h"

// ★追加：Run（走る）ステートを追加
enum class WeaponState { Idle, Walk, Run, Jump, Reload, Fire };

struct VfxEffect {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT3 endPosition;
	DirectX::XMFLOAT4 color;
	float lifeTime;
	float maxLifeTime;
	int type;
};

class Camera;
class PlayerControllerComponent;
class ModelComponent;
class AnimatorComponent;

class ViewModelComponent : public Component {
public:
	ViewModelComponent(Camera* camera, PlayerControllerComponent* playerCtrl);
	~ViewModelComponent() override = default;

	const char* GetName() const override { return "ViewModel"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	void SaveSettings();
	void LoadSettings();

private:
	Camera* camera = nullptr;
	PlayerControllerComponent* playerController = nullptr;
	ModelComponent* cachedModelComponent = nullptr;
	AnimatorComponent* animator = nullptr;

	WeaponState currentWeaponState = WeaponState::Idle;
	float actionTimer = 0.0f;

	int maxAmmo = 12;
	int currentAmmo = 12;

	DirectX::XMFLOAT3 offsetPos = { 0.1f, -0.2f, 0.3f };
	DirectX::XMFLOAT3 offsetRot = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

	DirectX::XMFLOAT3 muzzleOffset = { 0.1f, -0.1f, 0.5f };
	DirectX::XMFLOAT3 currentMuzzlePos = { 0, 0, 0 };
	bool showDebugMuzzle = false;

	std::vector<VfxEffect> vfxList;
	DirectX::XMFLOAT3 lastHitPos = { 0,0,0 };
	float hitMarkerTimer = 0.0f;

	char currentAnimName[64] = "Pistol_IDLE";
};