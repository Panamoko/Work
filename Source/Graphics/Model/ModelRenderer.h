#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Graphics/Model/Model.h"
#include "Graphics/ShaderClass/Shader.h"

enum class ShaderId
{
	Basic,
	Lambert,
	Pbr,
	Toon,

	EnumCount
};

class ModelRenderer
{
public:
	ModelRenderer(ID3D11Device* device);
	~ModelRenderer() {}

	void Draw(ShaderId shaderId, std::shared_ptr<Model> model);
	void Render(const RenderContext& rc);

	void SetIBL(
		ID3D11ShaderResourceView* irradiance,
		ID3D11ShaderResourceView* prefiltered,
		ID3D11ShaderResourceView* brdfLut)
	{
		iblIrradiance = irradiance;
		iblPrefiltered = prefiltered;
		iblBrdfLut = brdfLut;
	}
	bool enableIBL = true;

	// ---- Shadow settings (tunable) ----
	bool  enableShadow = true;
	float shadowRadius = 25.0f;
	float shadowDistance = 80.0f;
	float shadowNear = 1.0f;
	float shadowFar = 200.0f;
	float shadowStrength = 0.35f;   // direct-light brightness inside shadow (0=black, 1=none)
	float shadowBias = 0.0015f; // depth bias to remove acne

private:
	struct CbScene
	{
		DirectX::XMFLOAT4X4 viewProjection;
		DirectX::XMFLOAT4   lightDirection;
		DirectX::XMFLOAT4   lightColor;
		DirectX::XMFLOAT4   cameraPosition;
		DirectX::XMFLOAT4X4 lightViewProjection;
		DirectX::XMFLOAT4   shadowParams;
	};

	struct CbSkeleton
	{
		DirectX::XMFLOAT4X4 boneTransforms[256];
	};

	struct CbShadow
	{
		DirectX::XMFLOAT4X4 lightViewProjection;
	};

	struct DrawInfo
	{
		ShaderId              shaderId;
		std::shared_ptr<Model> model;
	};

	struct TransparencyDrawInfo
	{
		ShaderId           shaderId;
		const Model::Mesh* mesh;
		float              distance;
	};

	void InitShadow(ID3D11Device* device);
	void RenderShadowDepth(const RenderContext& rc);

	std::unique_ptr<Shader>           shaders[static_cast<int>(ShaderId::EnumCount)];
	std::unique_ptr<Shader>           toonOutlineShader;

	std::vector<DrawInfo>             drawInfos;
	std::vector<TransparencyDrawInfo> transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;

	ID3D11ShaderResourceView* iblIrradiance = nullptr;
	ID3D11ShaderResourceView* iblPrefiltered = nullptr;
	ID3D11ShaderResourceView* iblBrdfLut = nullptr;

	// ---- Shadow map resources ----
	UINT                                             shadowMapSize = 2048;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   shadowDSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowSRV;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>       shadowVS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>        shadowInputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>             shadowConstantBuffer;
	DirectX::XMFLOAT4X4                              lightViewProjection;
};