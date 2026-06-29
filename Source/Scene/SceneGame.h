#pragma once

#include <memory>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
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
	void InitSkybox(ID3D11Device* device);
	void DrawSkybox();

	Camera camera;
	FpsCameraController cameraController;

	std::shared_ptr<class Actor> stageActor;
	std::shared_ptr<class Actor> playerActor;
	std::shared_ptr<class Actor> viewModelActor;
	std::shared_ptr<class Actor> helmetActor;
	std::shared_ptr<class Actor> pbrSphereActor;

	std::shared_ptr<class Actor> selectedActor = nullptr;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradiance;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefiltered;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLut;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bulletHoleTex;

	// Procedural gradient sky
	Microsoft::WRL::ComPtr<ID3D11VertexShader> skyVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  skyPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       skyCB;

	// Sky colors / params (editable in Render Settings GUI)
	DirectX::XMFLOAT3 skyZenith = { 0.18f, 0.42f, 0.82f }; // top
	DirectX::XMFLOAT3 skyHorizon = { 0.80f, 0.88f, 0.96f }; // middle
	DirectX::XMFLOAT3 skyGround = { 0.32f, 0.34f, 0.38f }; // bottom
	float skyHorizonExp = 0.55f; // gradient sharpness (smaller = wider horizon band)
	float skyBands = 0.0f;  // 0 = smooth, >=2 = posterized (toon)
};