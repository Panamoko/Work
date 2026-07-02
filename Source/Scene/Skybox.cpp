#include "Scene/Skybox.h"
#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/RenderState.h"
#include "Camera/Camera.h"
#include <d3dcompiler.h>
#include <cstring>
#pragma comment(lib, "d3dcompiler.lib")

// Constant buffer for the procedural sky.
struct SkyboxCB
{
	DirectX::XMFLOAT4X4 invViewProj;
	DirectX::XMFLOAT4   cameraPos;
	DirectX::XMFLOAT4   zenithColor;
	DirectX::XMFLOAT4   horizonColor;
	DirectX::XMFLOAT4   groundColor;
	DirectX::XMFLOAT4   params; // x = horizonExp, y = bands
};

// Procedural gradient sky HLSL (compiled at runtime; no extra files).
static const char* kSkyboxHLSL = R"(
cbuffer SkyboxCB : register(b0)
{
	row_major float4x4 invViewProj;
	float4 cameraPos;
	float4 zenithColor;
	float4 horizonColor;
	float4 groundColor;
	float4 params; // x=horizonExp, y=bands
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float3 dir : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID)
{
	float2 uv = float2((id << 1) & 2, id & 2); // fullscreen triangle
	VSOut o;
	o.pos = float4(uv * 2.0f - 1.0f, 1.0f, 1.0f); // z = 1 (far)
	float4 world = mul(float4(o.pos.xy, 1.0f, 1.0f), invViewProj);
	o.dir = world.xyz / world.w - cameraPos.xyz;  // view ray in world space
	return o;
}

float4 PSMain(VSOut pin) : SV_TARGET
{
	float3 dir = normalize(pin.dir);
	float h = dir.y; // -1 (down) .. +1 (up)

	float horizonExp = max(params.x, 0.01f);

	float tu = pow(saturate(h), horizonExp);
	float3 upper = lerp(horizonColor.rgb, zenithColor.rgb, tu);

	float tl = pow(saturate(-h), horizonExp);
	float3 lower = lerp(horizonColor.rgb, groundColor.rgb, tl);

	float3 col = (h >= 0.0f) ? upper : lower;

	float bands = params.y;
	if (bands >= 2.0f)
	{
		col = floor(col * bands) / bands;
	}

	return float4(col, 1.0f);
}
)";

void Skybox::Initialize(ID3D11Device* device)
{
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(SkyboxCB), cb.GetAddressOf());

	UINT flags = 0;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	size_t len = strlen(kSkyboxHLSL);

	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

	HRESULT hr = D3DCompile(kSkyboxHLSL, len, "Skybox", nullptr, nullptr,
		"VSMain", "vs_5_0", flags, 0, vsBlob.GetAddressOf(), errBlob.GetAddressOf());
	if (FAILED(hr))
	{
		if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
		return;
	}
	device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf());

	hr = D3DCompile(kSkyboxHLSL, len, "Skybox", nullptr, nullptr,
		"PSMain", "ps_5_0", flags, 0, psBlob.GetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
		return;
	}
	device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf());
}

void Skybox::Draw(const Camera& camera)
{
	if (!vs || !ps) return;

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* rs = Graphics::Instance().GetRenderState();

	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&camera.GetView());
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&camera.GetProjection());
	DirectX::XMMATRIX invVP = DirectX::XMMatrixInverse(nullptr, V * P);

	SkyboxCB data{};
	DirectX::XMStoreFloat4x4(&data.invViewProj, invVP);
	const DirectX::XMFLOAT3& eye = camera.GetEye();
	data.cameraPos = { eye.x, eye.y, eye.z, 1.0f };
	data.zenithColor = { zenith.x,  zenith.y,  zenith.z,  1.0f };
	data.horizonColor = { horizon.x, horizon.y, horizon.z, 1.0f };
	data.groundColor = { ground.x,  ground.y,  ground.z,  1.0f };
	data.params = { horizonExp, bands, 0.0f, 0.0f };
	dc->UpdateSubresource(cb.Get(), 0, 0, &data, 0, 0);

	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

	ID3D11Buffer* nullVB = nullptr; UINT zero = 0;
	dc->IASetInputLayout(nullptr);
	dc->IASetVertexBuffers(0, 1, &nullVB, &zero, &zero);
	dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dc->VSSetShader(vs.Get(), nullptr, 0);
	dc->PSSetShader(ps.Get(), nullptr, 0);
	dc->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
	dc->PSSetConstantBuffers(0, 1, cb.GetAddressOf());

	dc->Draw(3, 0);

	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
}
