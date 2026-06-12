#pragma once

#include <DirectXMath.h>

// カメラ
class Camera
{
public:
	Camera();

	// 指定方向を向く
	void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);

	// パースペクティブ設定
	void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);

	// ★ モダンC++: 値を返すだけの関数には [[nodiscard]] をつけ、戻り値の受け取り忘れ（バグ）をコンパイル時に防ぐ
	[[nodiscard]] const DirectX::XMFLOAT4X4& GetView() const { return view; }
	[[nodiscard]] const DirectX::XMFLOAT4X4& GetProjection() const { return projection; }

	[[nodiscard]] const DirectX::XMFLOAT3& GetEye() const { return eye; }
	[[nodiscard]] const DirectX::XMFLOAT3& GetFocus() const { return focus; }

	[[nodiscard]] const DirectX::XMFLOAT3& GetUp() const { return up; }
	[[nodiscard]] const DirectX::XMFLOAT3& GetFront() const { return front; }
	[[nodiscard]] const DirectX::XMFLOAT3& GetRight() const { return right; }

private:
	DirectX::XMFLOAT4X4		view;
	DirectX::XMFLOAT4X4		projection;

	DirectX::XMFLOAT3		eye;
	DirectX::XMFLOAT3		focus;

	DirectX::XMFLOAT3		up;
	DirectX::XMFLOAT3		front;
	DirectX::XMFLOAT3		right;
};