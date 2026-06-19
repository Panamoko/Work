#pragma once

#include "GameObject/Component.h"
#include "Camera/Camera.h"
#include <DirectXMath.h>
#include <vector> // vectorがインクルードされているか確認

class ViewModelComponent : public Component
{
public:
	ViewModelComponent(Camera* camera, class PlayerControllerComponent* playerCtrl);
	~ViewModelComponent() override = default;

	const char* GetName() const override { return "ViewModel"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	void SaveSettings();
	void LoadSettings();

private:
	Camera* camera = nullptr;
	class PlayerControllerComponent* playerController = nullptr;

	class ModelComponent* cachedModelComponent = nullptr;
	class AnimatorComponent* animator = nullptr;

	DirectX::XMFLOAT3 offsetPos = { 0.0f, -0.2f, 0.3f };
	DirectX::XMFLOAT3 offsetRot = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 offsetScale = { 0.01f, 0.01f, 0.01f };

	char currentAnimName[128] = "Pistol_IDLE";

	enum class WeaponState { Idle, Walk, Jump, Fire, Reload };
	WeaponState currentWeaponState = WeaponState::Idle;

	float actionTimer = 0.0f;

	int maxAmmo = 12;
	int currentAmmo = 12;

	DirectX::XMFLOAT3 lastHitPos = { 0, 0, 0 };
	float hitMarkerTimer = 0.0f;

	struct VfxEffect {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT3 endPosition;
		DirectX::XMFLOAT4 color;
		float lifeTime;
		float maxLifeTime;
		int type;
	};
	std::vector<VfxEffect> vfxList;

	DirectX::XMFLOAT3 muzzleOffset = { 0.0f, 0.1f, 0.4f };

	// =======================================================
	// ★追加：銃口位置のデバッグ表示フラグ
	// =======================================================
	bool showDebugMuzzle = true;
	DirectX::XMFLOAT3 currentMuzzlePos = { 0, 0, 0 };
};