#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class PrimitiveRenderer
{
public:
	PrimitiveRenderer(ID3D11Device* device);

	// ★修正：第3引数にテクスチャUV座標（デフォルト値 0,0）を追加
	void AddVertex(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT2& texcoord = { 0.0f, 0.0f });

	// 軸描画(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawAxis(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

	// グリッド描画(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawGrid(int subdivisions, float scale);

	// ★追加：描画するテクスチャをセットする機能
	void SetTexture(ID3D11ShaderResourceView* texture) { currentTexture = texture; }

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection,
		D3D11_PRIMITIVE_TOPOLOGY primitiveTopology);

private:
	static const UINT VertexCapacity = 3 * 1024;

	struct CbScene
	{
		DirectX::XMFLOAT4X4		viewProjection;
		int						useTexture; // ★追加：テクスチャを使うかどうかのフラグ
		float					pad[3];     // 16バイト境界に合わせるためのパディング
	};

	struct Vertex
	{
		DirectX::XMFLOAT3	position;
		DirectX::XMFLOAT4	color;
		DirectX::XMFLOAT2	texcoord; // ★追加：UV座標
	};
	std::vector<Vertex>		vertices;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>			vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>			constantBuffer;

	// ★追加：テクスチャ描画用のサンプラと現在セットされているテクスチャ
	Microsoft::WRL::ComPtr<ID3D11SamplerState>		samplerState;
	ID3D11ShaderResourceView* currentTexture = nullptr;
};