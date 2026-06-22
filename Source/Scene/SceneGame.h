#pragma once

#include <memory>
#include <wrl.h>         // ★追加: ComPtr用
#include <d3d11.h>       // ★追加: ID3D11ShaderResourceView用
#include "Scene.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"

class SceneGame : public Scene
{
public:
	SceneGame();
	~SceneGame() override;

	void Update(float elapsedTime) override;
	void Render(float elapsedTime) override;
	void DrawGUI() override;

private:
	Camera camera;
	FpsCameraController cameraController;

	std::shared_ptr<class Actor> stageActor;
	std::shared_ptr<class Actor> playerActor;

	// 都築の修正：WeaponActorを廃止し、FPS専用のViewModelActorに変更
	std::shared_ptr<class Actor> viewModelActor;

	// PBR確認用：DamagedHelmet
	std::shared_ptr<class Actor> helmetActor;
	std::shared_ptr<class Actor> pbrSphereActor;

	// =======================================================
	// ★追加：IBL（Image-Based Lighting）用のテクスチャ保持
	// =======================================================
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradiance;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefiltered;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLut;
};