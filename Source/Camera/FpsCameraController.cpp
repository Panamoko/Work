#include <imgui.h>
#include <algorithm>
#include "Camera/FpsCameraController.h"

FpsCameraController::FpsCameraController() : angleX(0.0f), angleY(0.0f), isFpsMode(false) {}

void FpsCameraController::SyncCameraToController(const Camera& camera)
{
	const DirectX::XMFLOAT3& frontVec = camera.GetFront();
	angleX = ::asinf(-frontVec.y);
	angleY = ::atan2f(-frontVec.x, -frontVec.z);
}

void FpsCameraController::SyncControllerToCamera(Camera& camera)
{
	camera.SetLookAt(eye, focus, up);
}

void FpsCameraController::Update(HWND hWnd, const DirectX::XMFLOAT3& playerPosition, float eyeHeight)
{
	// 右クリックでFPSモード切り替え
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		isFpsMode = true;
		ShowCursor(FALSE);

		// 【重要】切り替え直後にカーソルを中央にワープさせ、初回の暴走を防ぐ
		RECT rect;
		GetClientRect(hWnd, &rect);
		POINT center = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
		ClientToScreen(hWnd, &center);
		SetCursorPos(center.x, center.y);
	}
	else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		isFpsMode = false;
		ShowCursor(TRUE);
	}

	if (isFpsMode)
	{
		// 【罠の解消】ImGuiを完全に無視し、OS(Win32)から直接生のカーソル座標を取得する
		POINT pt;
		GetCursorPos(&pt);

		// ウィンドウ中央のスクリーン座標を計算
		RECT rect;
		GetClientRect(hWnd, &rect);
		POINT center = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
		ClientToScreen(hWnd, &center);

		// 中央からの移動ピクセル数を直接算出
		float moveX = static_cast<float>(pt.x - center.x) * 0.002f; // 0.002fはマウス感度
		float moveY = static_cast<float>(pt.y - center.y) * 0.002f;

		// 視点の角度に加算
		angleY += moveX;
		angleX += moveY;

		// ピッチ角（上下）の制限
		const float limitX = DirectX::XM_PIDIV2 - 0.01f;
		angleX = std::clamp(angleX, -limitX, limitX);

		// カーソルを物理的に中央に戻す（次回もptとcenterの差分だけで計算できる）
		SetCursorPos(center.x, center.y);
	}

	// 目の高さにカメラを配置
	eye = playerPosition;
	eye.y += eyeHeight;

	// 角度からフロントベクトルを算出
	float sx = ::sinf(angleX);
	float cx = ::cosf(angleX);
	float sy = ::sinf(angleY);
	float cy = ::cosf(angleY);

	DirectX::XMVECTOR Front = DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f);
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&eye);
	DirectX::XMVECTOR Focus = DirectX::XMVectorAdd(Eye, Front);

	DirectX::XMStoreFloat3(&focus, Focus);
	up = { 0, 1, 0 };
}