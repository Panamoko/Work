#pragma once

#include <memory>
#include "Scene/Scene.h"
#include "Camera/Camera.h"
#include "Camera/FpsCameraController.h"
#include "GameObject/Actor.h"

class SceneGame : public Scene
{
public:
	SceneGame();
	~SceneGame() override;

	void Update(float elapsedTime) override;
	void Render(float elapsedTime) override;
	void DrawGUI() override;

private:
	Camera								camera;
	FpsCameraController					cameraController;

	std::shared_ptr<Actor>				stageActor;
	std::shared_ptr<Actor>				playerActor;
};