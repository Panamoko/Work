#pragma once
#include <windows.h>
#include "Camera/Camera.h"

class FpsCameraController
{
public:
    FpsCameraController();

    void SyncCameraToController(const Camera& camera);
    void SyncControllerToCamera(Camera& camera);

    // FPS操作の更新。右クリック中だけFPSモードになるようにする
    void Update(HWND hWnd, const DirectX::XMFLOAT3& playerPosition, float eyeHeight);

private:
    DirectX::XMFLOAT3 eye;
    DirectX::XMFLOAT3 focus;
    DirectX::XMFLOAT3 up;
    DirectX::XMFLOAT3 front;

    float angleX; // ピッチ（上下）
    float angleY; // ヨー（左右）

    bool isFpsMode; // FPS操作中かどうか
};