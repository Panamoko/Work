#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Scene.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"
#include "Scene/Skybox.h"
#include "Scene/EditorCamera.h"
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
	enum class EditorMode { Edit, Play };

	// Content drawer folder tree
	struct AssetNode
	{
		std::map<std::string, AssetNode> folders;
		std::vector<std::string> files; // full paths
	};

	void StartPlay();
	void StopPlay();
	void SnapshotTransforms();
	void RestoreTransforms();
	void ApplyCameraView(); // FPS/TPS camera + head-mesh visibility

	void ScanAssets();
	void BuildAssetTree();
	void DrawAssetNode(const AssetNode& node, const char* filter);
	void SpawnAsset(const std::string& path);

	void SaveScene();
	void LoadScene();
	void ClearScene();

	bool ScreenRay(DirectX::XMFLOAT3& outOrigin, DirectX::XMFLOAT3& outDir) const;
	bool ScreenRayToPlane(float planeY, DirectX::XMFLOAT3& outHit) const;
	void PaintTileAtMouse();
	void EraseTileAtMouse();
	void SaveTilemap();
	void LoadTilemap();
	void ClearTilemap();

	bool GetActorAABB(const std::shared_ptr<class Actor>& actor, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax);
	bool RayActorAABB(const std::shared_ptr<class Actor>& actor, const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD, float& outDist);
	std::shared_ptr<class Actor> PickActor(const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD);
	void UpdateGizmo();
	void DrawImGuizmo();
	void DrawSelectionOutline(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

	void BuildNavGrid();
	void DrawNavMeshDebug(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

	Camera camera;
	FpsCameraController cameraController;
	EditorCamera editorCam;

	EditorMode mode = EditorMode::Edit;
	int gizmoMode = 0; // 0 = translate, 1 = rotate, 2 = scale

	// ---- Play camera view (F5 toggles) ----
	int   cameraView = 0;      // 0 = FPS, 1 = TPS
	bool  prevF5 = false;
	float tpsDistance = 4.5f;
	float tpsHeight = 1.6f;

	std::map<class Model*, std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3>> aabbCache;

	struct ActorSnapshot
	{
		std::shared_ptr<class Actor> actor;
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 rot;
		DirectX::XMFLOAT3 scale;
	};
	std::vector<ActorSnapshot> snapshots;

	// Content drawer
	std::vector<std::string> assetPaths;
	bool assetsScanned = false;   // Content Browser èââÒï`âÊéûÇ… ScanAssets Ç∑ÇÈ
	AssetNode assetTree;
	int spawnType = 0; // 0 = Prop, 1 = Enemy
	std::vector<std::shared_ptr<class Actor>> spawnedActors;

	// Tilemap
	bool  paintMode = false;
	bool  eraseMode = false;
	std::string activeTilePath;
	float tileSize = 2.0f;
	float paintY = 0.0f;
	struct TileEntry { std::shared_ptr<class Actor> actor; std::string path; };
	std::map<std::pair<int, int>, TileEntry> tiles;

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

	CollisionMesh stageCollision;
	NavGrid navGrid;
	float navCellSize = 1.0f;
	float navAgentHeight = 1.0f;
	float navMaxStep = 0.4f;
	bool  showNav = false;
	float navDebugRadius = 25.0f;
};