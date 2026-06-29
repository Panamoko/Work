#pragma once

#include <wrl.h>
#include <d3d11.h>

// Sampler states
enum class SamplerState
{
	PointWrap,
	PointClamp,
	LinearWrap,
	LinearClamp,
	ShadowCompare, // comparison sampler for shadow mapping (PCF)

	EnumCount
};

// Depth-stencil states
enum class DepthState
{
	TestAndWrite,
	TestOnly,
	WriteOnly,
	NoTestNoWrite,

	EnumCount
};

// Blend states
enum class BlendState
{
	Opaque,
	Transparency,
	Additive,
	Subtraction,
	Multiply,

	EnumCount
};

// Rasterizer states
enum class RasterizerState
{
	SolidCullNone,
	SolidCullBack,
	SolidCullFront, // for toon outline (front-face culling)
	WireCullNone,
	WireCullBack,
	ShadowDepth,    // solid + depth bias, for the shadow depth pass

	EnumCount
};

// Render state container
class RenderState
{
public:
	RenderState(ID3D11Device* device);
	~RenderState() = default;

	ID3D11SamplerState* GetSamplerState(SamplerState state) const
	{
		return samplerStates[static_cast<int>(state)].Get();
	}

	ID3D11DepthStencilState* GetDepthStencilState(DepthState state) const
	{
		return depthStencilStates[static_cast<int>(state)].Get();
	}

	ID3D11BlendState* GetBlendState(BlendState state) const
	{
		return blendStates[static_cast<int>(state)].Get();
	}

	ID3D11RasterizerState* GetRasterizerState(RasterizerState state) const
	{
		return rasterizerStates[static_cast<int>(state)].Get();
	}

private:
	Microsoft::WRL::ComPtr<ID3D11SamplerState>		samplerStates[static_cast<int>(SamplerState::EnumCount)];
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilStates[static_cast<int>(DepthState::EnumCount)];
	Microsoft::WRL::ComPtr<ID3D11BlendState>		blendStates[static_cast<int>(BlendState::EnumCount)];
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>	rasterizerStates[static_cast<int>(RasterizerState::EnumCount)];
};