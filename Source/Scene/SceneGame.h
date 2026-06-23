#pragma once

#include <memory>
#include <wrl.h>         
#include <d3d11.h>       
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
	std::shared_ptr<class Actor> viewModelActor;
	std::shared_ptr<class Actor> helmetActor;
	std::shared_ptr<class Actor> pbrSphereActor;

	// =======================================================
	// エディタGUI用の「現在選択中のアクター」保持ポインタ
	// =======================================================
	std::shared_ptr<class Actor> selectedActor = nullptr;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradiance;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefiltered;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLut;

	// ★追加：弾痕テクスチャ
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bulletHoleTex;
};