#include "Camera/Camera.h"

// コンストラクタ
Camera::Camera()
{
	// ★ 注意喚起: ここでのアスペクト比固定は仮の処置。
	// ゲームの解像度設定やウィンドウサイズ変更時に、
	// 必ず SetPerspectiveFov が外部から再コールされる設計にすること。
	SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		1280.0f / 720.0f,					// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	SetLookAt(
		{ 0, 0, -5 },		// 視点
		{ 0, 0, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
}

// 指定方向を向く
void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
	// 視点、注視点、上方向からビュー行列を作成
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&eye);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&focus);
	DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&up);
	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Eye, Focus, Up);
	DirectX::XMStoreFloat4x4(&view, View);

	// ★ プロの最適化: XMMatrixInverse という超重い処理を削除。
	// ビュー行列の左上3x3は「直交行列」であり、その各行(Row)には
	// 既にカメラの Right, Up, Front ベクトルがそのまま格納されている。
	// これを直接抜き出すだけで済むため、計算コストが劇的に下がる。
	this->right = { view._11, view._21, view._31 };
	this->up = { view._12, view._22, view._32 };
	this->front = { view._13, view._23, view._33 };

	// 視点、注視点を保存
	this->eye = eye;
	this->focus = focus;
}

// パースペクティブ設定
void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
	// 画角、画面比率、クリップ距離からプロジェクション行列を作成
	DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	DirectX::XMStoreFloat4x4(&projection, Projection);
}