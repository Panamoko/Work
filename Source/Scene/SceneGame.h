#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Scene.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"
#include "Scene/Skybox.h"
#include "Physics/CollisionMesh.h"
#include "AI/NavGrid.h"

class SceneGame : public Scene
{
public:
	SceneGame();
	~SceneGame() override;

	void Update(float elapsedTime) override;
	void Render(float elapsedTime) override;
	void DrawGUI() override;

private:
	void BuildNavGrid();
	void DrawNavMeshDebug(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

	Camera camera;
	FpsCameraController cameraController;

	std::shared_ptr<class Actor> stageActor;
	std::shared_ptr<class Actor> playerActor;
	std::shared_ptr<class Actor> viewModelActor;
	std::shared_ptr<class Actor> helmetActor;
	std::shared_ptr<class Actor> pbrSphereActor;
	std::shared_ptr<class Actor> enemyActor;

	std::shared_ptr<class Actor> selectedActor = nullptr;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradiance;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefiltered;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLut;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bulletHoleTex;

	Skybox skybox;

	// ---- Shared navigation (one nav mesh per stage) ----
	CollisionMesh stageCollision;
	NavGrid navGrid;
	float navCellSize = 1.0f;
	float navAgentHeight = 1.0f;
	float navMaxStep = 0.4f;   // biggest floor height difference the agent can cross
	bool  showNav = false;
	float navDebugRadius = 25.0f;
};