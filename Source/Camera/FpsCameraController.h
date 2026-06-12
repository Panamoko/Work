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
    DirectX::XMFLOAT3 eye = { 0, 0, 0 };   // ★ 初期化漏れ防止
    DirectX::XMFLOAT3 focus = { 0, 0, 0 }; // ★ 初期化漏れ防止
    DirectX::XMFLOAT3 up = { 0, 1, 0 };    // ★ 初期化漏れ防止
    DirectX::XMFLOAT3 front = { 0, 0, 1 }; // ★ 初期化漏れ防止

    float angleX = 0.0f; // ピッチ（上下）
    float angleY = 0.0f; // ヨー（左右）

    bool isFpsMode = false; // FPS操作中かどうか
};