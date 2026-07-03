#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class Camera;

// Procedural gradient sky, drawn as a single fullscreen triangle.
// The shader is compiled at runtime from an embedded HLSL source, so no
// external .hlsl file is needed.
class Skybox
{
public:
	void Initialize(ID3D11Device* device);
	void Draw(const Camera& camera);

	// Gradient parameters (edited from the Render Settings GUI).
	DirectX::XMFLOAT3 zenith = { 0.18f, 0.42f, 0.82f };
	DirectX::XMFLOAT3 horizon = { 0.80f, 0.88f, 0.96f };
	DirectX::XMFLOAT3 ground = { 0.32f, 0.34f, 0.38f };
	float horizonExp = 0.55f;
	float bands = 0.0f;

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       cb;
};