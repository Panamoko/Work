#pragma once

#include <memory>
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
};