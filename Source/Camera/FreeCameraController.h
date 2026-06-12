#pragma once
#include "Camera/Camera.h"

class FreeCameraController
{
public:
	// カメラからコントローラーへパラメータを同期する
	void SyncCameraToController(const Camera& camera);

	// コントローラーからカメラへパラメータを同期する
	void SyncControllerToCamera(Camera& camera);

	// 更新処理
	void Update();

private:
	DirectX::XMFLOAT3		eye = { 0, 0, 0 };
	DirectX::XMFLOAT3		focus = { 0, 0, 0 };
	DirectX::XMFLOAT3		up = { 0, 1, 0 };
	DirectX::XMFLOAT3		right = { 1, 0, 0 };
	float					distance = 5.0f;

	float					angleX = 0.0f;
	float					angleY = 0.0f;
};