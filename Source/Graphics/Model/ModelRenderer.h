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
	Toon, // ★追加：アニメ調（NPR-PBR）用シェーダー

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
private:
	struct CbScene
	{
		DirectX::XMFLOAT4X4 viewProjection;
		DirectX::XMFLOAT4   lightDirection;
		DirectX::XMFLOAT4   lightColor;
		DirectX::XMFLOAT4   cameraPosition;
	};

	struct CbSkeleton
	{
		DirectX::XMFLOAT4X4 boneTransforms[256];
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

	std::unique_ptr<Shader>           shaders[static_cast<int>(ShaderId::EnumCount)];
	std::unique_ptr<Shader>           toonOutlineShader; // ★追加：アウトライン描画専用シェーダー

	std::vector<DrawInfo>             drawInfos;
	std::vector<TransparencyDrawInfo> transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;

	ID3D11ShaderResourceView* iblIrradiance = nullptr;
	ID3D11ShaderResourceView* iblPrefiltered = nullptr;
	ID3D11ShaderResourceView* iblBrdfLut = nullptr;
};