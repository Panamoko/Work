#include "Core/Misc.h"
#include "Graphics/Graphics.h"
#include <d3dcompiler.h>
#include "Graphics/GpuResourceUtils.h"
#pragma comment(lib, "d3dcompiler.lib")
void Graphics::Initialize(HWND hWnd)
{
	this->hWnd = hWnd;
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT screenWidth = rc.right - rc.left;
	UINT screenHeight = rc.bottom - rc.top;
	this->screenWidth = static_cast<float>(screenWidth);
	this->screenHeight = static_cast<float>(screenHeight);

	HRESULT hr = S_OK;

	{
		UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
		};

		DXGI_SWAP_CHAIN_DESC swapchainDesc;
		swapchainDesc.BufferDesc.Width = screenWidth;
		swapchainDesc.BufferDesc.Height = screenHeight;
		swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = 2;
		swapchainDesc.OutputWindow = hWnd;
		swapchainDesc.Windowed = TRUE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.Flags = 0;

		D3D_FEATURE_LEVEL featureLevel;
		hr = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION, &swapchainDesc,
			swapchain.GetAddressOf(), device.GetAddressOf(),
			&featureLevel, immediateContext.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(texture2d.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		hr = device->CreateRenderTargetView(texture2d.Get(), nullptr, renderTargetView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		D3D11_TEXTURE2D_DESC texture2dDesc;
		texture2dDesc.Width = screenWidth;
		texture2dDesc.Height = screenHeight;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.ArraySize = 1;
		texture2dDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.SampleDesc.Quality = 0;
		texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
		texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texture2dDesc.CPUAccessFlags = 0;
		texture2dDesc.MiscFlags = 0;
		hr = device->CreateTexture2D(&texture2dDesc, nullptr, texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		hr = device->CreateDepthStencilView(texture2d.Get(), nullptr, depthStencilView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	viewport.Width = static_cast<float>(screenWidth);
	viewport.Height = static_cast<float>(screenHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	renderState = std::make_unique<RenderState>(device.Get());
	primitiveRenderer = std::make_unique<PrimitiveRenderer>(device.Get());
	shapeRenderer = std::make_unique<ShapeRenderer>(device.Get());
	modelRenderer = std::make_unique<ModelRenderer>(device.Get());

	// IBLƒeƒNƒXƒ`ƒƒ‚ð“Ç‚Ýž‚ñ‚ÅModelRenderer‚É“o˜^
	GpuResourceUtils::LoadTexture(device.Get(),
		"Data/Sprite/IBL/diffuse_iem.dds", iblIrradianceSRV.GetAddressOf());
	GpuResourceUtils::LoadTexture(device.Get(),
		"Data/Sprite/IBL/specular_pmrem.dds", iblPrefilteredSRV.GetAddressOf());
	GpuResourceUtils::LoadTexture(device.Get(),
		"Data/Sprite/IBL/lut_ggx.dds", iblBrdfLutSRV.GetAddressOf());

	modelRenderer->SetIBL(
		iblIrradianceSRV.Get(),
		iblPrefilteredSRV.Get(),
		iblBrdfLutSRV.Get());
}

void Graphics::Clear(float r, float g, float b, float a)
{
	float color[4]{ r, g, b, a };
	immediateContext->ClearRenderTargetView(renderTargetView.Get(), color);
	immediateContext->ClearDepthStencilView(depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Graphics::SetRenderTargets()
{
	immediateContext->RSSetViewports(1, &viewport);
	immediateContext->OMSetRenderTargets(1,
		renderTargetView.GetAddressOf(), depthStencilView.Get());
}

void Graphics::Present(UINT syncInterval)
{
	swapchain->Present(syncInterval, 0);
}

void Graphics::Resize(UINT width, UINT height)
{
	if (!swapchain || width == 0 || height == 0) return;

	immediateContext->OMSetRenderTargets(0, nullptr, nullptr);
	renderTargetView.Reset();
	depthStencilView.Reset();

	HRESULT hr = swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	screenWidth = static_cast<float>(width);
	screenHeight = static_cast<float>(height);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBuffer;
	hr = device->CreateTexture2D(&depthDesc, nullptr, depthBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	hr = device->CreateDepthStencilView(depthBuffer.Get(), nullptr, depthStencilView.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
}


// =======================================================
// Post-process: bloom + tone mapping / grading
// =======================================================
struct PostCB
{
	float exposure;
	float contrast;
	float saturation;
	float tonemapEnable;
	float bloomIntensity;
	float vignette;
	float vignetteRadius;
	float _pad;
	float tint[4]; // rgb + unused
};

struct BloomCB
{
	float texelSize[2];
	float direction[2];
	float threshold;
	float intensity;
	float _pad[2];
};

static const char* kPostHLSL = R"(
cbuffer PostCB : register(b0)
{
	float exposure;
	float contrast;
	float saturation;
	float tonemapEnable;
	float bloomIntensity;
	float vignette;
	float vignetteRadius;
	float _pad;
	float4 tint;
};

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(uint id : SV_VertexID)
{
	float2 uv = float2((id << 1) & 2, id & 2);
	VSOut o;
	o.uv = uv;
	o.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return o;
}

Texture2D    SceneTex : register(t0);
Texture2D    BloomTex : register(t1);
SamplerState Samp     : register(s0);

float3 ACES(float3 x)
{
	const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(VSOut pin) : SV_TARGET
{
	float3 color = SceneTex.Sample(Samp, pin.uv).rgb;
	float3 bloom = BloomTex.Sample(Samp, pin.uv).rgb;

	color += bloom * bloomIntensity;   // add bloom in HDR
	color *= exposure;

	if (tonemapEnable > 0.5f)
		color = ACES(color);
	else
		color = saturate(color);

	color = saturate((color - 0.5f) * contrast + 0.5f);

	float luma = dot(color, float3(0.299f, 0.587f, 0.114f));
	color = lerp(float3(luma, luma, luma), color, saturation);

	// color grading tint
	color *= tint.rgb;

	// vignette (darken corners)
	float dist = distance(pin.uv, float2(0.5f, 0.5f)) * 1.41421f; // 0 center .. ~1 corner
	float vig = smoothstep(vignetteRadius, 1.0f, dist);
	color *= (1.0f - vig * vignette);

	return float4(saturate(color), 1.0f);
}
)";

static const char* kBloomHLSL = R"(
cbuffer BloomCB : register(b0)
{
	float2 texelSize;
	float2 direction;
	float  threshold;
	float  intensity;
	float2 _pad;
};

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(uint id : SV_VertexID)
{
	float2 uv = float2((id << 1) & 2, id & 2);
	VSOut o;
	o.uv = uv;
	o.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return o;
}

Texture2D    SrcTex : register(t0);
SamplerState Samp   : register(s0);

// extract bright pixels above threshold
float4 BrightPS(VSOut pin) : SV_TARGET
{
	float3 c = SrcTex.Sample(Samp, pin.uv).rgb;
	float luma = dot(c, float3(0.299f, 0.587f, 0.114f));
	float contrib = max(luma - threshold, 0.0f) / max(luma, 1e-4f);
	return float4(c * contrib, 1.0f);
}

// separable gaussian blur (direction = (1,0) or (0,1))
float4 BlurPS(VSOut pin) : SV_TARGET
{
	const float w0 = 0.227027f;
	const float w1 = 0.1945946f;
	const float w2 = 0.1216216f;
	const float w3 = 0.054054f;
	const float w4 = 0.016216f;

	float3 result = SrcTex.Sample(Samp, pin.uv).rgb * w0;
	float2 o1 = direction * texelSize * 1.0f;
	float2 o2 = direction * texelSize * 2.0f;
	float2 o3 = direction * texelSize * 3.0f;
	float2 o4 = direction * texelSize * 4.0f;

	result += SrcTex.Sample(Samp, pin.uv + o1).rgb * w1;
	result += SrcTex.Sample(Samp, pin.uv - o1).rgb * w1;
	result += SrcTex.Sample(Samp, pin.uv + o2).rgb * w2;
	result += SrcTex.Sample(Samp, pin.uv - o2).rgb * w2;
	result += SrcTex.Sample(Samp, pin.uv + o3).rgb * w3;
	result += SrcTex.Sample(Samp, pin.uv - o3).rgb * w3;
	result += SrcTex.Sample(Samp, pin.uv + o4).rgb * w4;
	result += SrcTex.Sample(Samp, pin.uv - o4).rgb * w4;

	return float4(result, 1.0f);
}
)";

static Microsoft::WRL::ComPtr<ID3DBlob> CompilePost(const char* src, const char* entry, const char* target)
{
	UINT flags = 0;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	Microsoft::WRL::ComPtr<ID3DBlob> blob, err;
	HRESULT hr = D3DCompile(src, strlen(src), "Post", nullptr, nullptr, entry, target, flags, 0, blob.GetAddressOf(), err.GetAddressOf());
	if (FAILED(hr) && err) OutputDebugStringA(static_cast<const char*>(err->GetBufferPointer()));
	return blob;
}

static void MakeColorTarget(ID3D11Device* device, UINT w, UINT h,
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
{
	rtv.Reset(); srv.Reset();
	D3D11_TEXTURE2D_DESC td{};
	td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	if (FAILED(device->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) return;
	device->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf());
	device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
}

void Graphics::EnsureSceneTarget()
{
	UINT w = static_cast<UINT>(screenWidth);
	UINT h = static_cast<UINT>(screenHeight);
	if (w == 0 || h == 0) return;
	if (sceneRTV && sceneTargetWidth == w && sceneTargetHeight == h) return;

	MakeColorTarget(device.Get(), w, h, sceneRTV, sceneSRV);
	sceneTargetWidth = w;
	sceneTargetHeight = h;

	bloomWidth = (w / 2 > 0) ? w / 2 : 1;
	bloomHeight = (h / 2 > 0) ? h / 2 : 1;
	MakeColorTarget(device.Get(), bloomWidth, bloomHeight, bloomA_RTV, bloomA_SRV);
	MakeColorTarget(device.Get(), bloomWidth, bloomHeight, bloomB_RTV, bloomB_SRV);
}

void Graphics::InitPostProcess()
{
	if (postVS && postPS) return;

	auto postVSBlob = CompilePost(kPostHLSL, "VSMain", "vs_5_0");
	auto postPSBlob = CompilePost(kPostHLSL, "PSMain", "ps_5_0");
	if (!postVSBlob || !postPSBlob) return;
	device->CreateVertexShader(postVSBlob->GetBufferPointer(), postVSBlob->GetBufferSize(), nullptr, postVS.GetAddressOf());
	device->CreatePixelShader(postPSBlob->GetBufferPointer(), postPSBlob->GetBufferSize(), nullptr, postPS.GetAddressOf());

	auto bloomVSBlob = CompilePost(kBloomHLSL, "VSMain", "vs_5_0");
	auto brightBlob = CompilePost(kBloomHLSL, "BrightPS", "ps_5_0");
	auto blurBlob = CompilePost(kBloomHLSL, "BlurPS", "ps_5_0");
	if (bloomVSBlob) device->CreateVertexShader(bloomVSBlob->GetBufferPointer(), bloomVSBlob->GetBufferSize(), nullptr, bloomVS.GetAddressOf());
	if (brightBlob)  device->CreatePixelShader(brightBlob->GetBufferPointer(), brightBlob->GetBufferSize(), nullptr, brightPS.GetAddressOf());
	if (blurBlob)    device->CreatePixelShader(blurBlob->GetBufferPointer(), blurBlob->GetBufferSize(), nullptr, blurPS.GetAddressOf());

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(PostCB);
	device->CreateBuffer(&bd, nullptr, postCB.GetAddressOf());
	bd.ByteWidth = sizeof(BloomCB);
	device->CreateBuffer(&bd, nullptr, bloomCB.GetAddressOf());
}

void Graphics::RenderBloom()
{
	if (!bloomA_RTV || !bloomVS || !brightPS || !blurPS) return;

	immediateContext->IASetInputLayout(nullptr);
	ID3D11Buffer* nullVB = nullptr; UINT zero = 0;
	immediateContext->IASetVertexBuffers(0, 1, &nullVB, &zero, &zero);
	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediateContext->VSSetShader(bloomVS.Get(), nullptr, 0);
	immediateContext->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	immediateContext->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	immediateContext->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	ID3D11SamplerState* samp = renderState->GetSamplerState(SamplerState::LinearClamp);
	immediateContext->PSSetSamplers(0, 1, &samp);

	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(bloomWidth);
	vp.Height = static_cast<float>(bloomHeight);
	vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
	immediateContext->RSSetViewports(1, &vp);

	BloomCB cb{};
	cb.threshold = postBloomThreshold;
	cb.intensity = postBloomIntensity;
	cb.texelSize[0] = 1.0f / static_cast<float>(bloomWidth);
	cb.texelSize[1] = 1.0f / static_cast<float>(bloomHeight);

	ID3D11ShaderResourceView* nullSrv = nullptr;

	// Bright pass: scene -> bloomA
	immediateContext->PSSetShaderResources(0, 1, &nullSrv);
	immediateContext->OMSetRenderTargets(1, bloomA_RTV.GetAddressOf(), nullptr);
	cb.direction[0] = 0.0f; cb.direction[1] = 0.0f;
	immediateContext->UpdateSubresource(bloomCB.Get(), 0, 0, &cb, 0, 0);
	immediateContext->PSSetConstantBuffers(0, 1, bloomCB.GetAddressOf());
	immediateContext->PSSetShader(brightPS.Get(), nullptr, 0);
	{ ID3D11ShaderResourceView* s = sceneSRV.Get(); immediateContext->PSSetShaderResources(0, 1, &s); }
	immediateContext->Draw(3, 0);

	// Blur horizontal: bloomA -> bloomB
	immediateContext->PSSetShaderResources(0, 1, &nullSrv);
	immediateContext->OMSetRenderTargets(1, bloomB_RTV.GetAddressOf(), nullptr);
	cb.direction[0] = 1.0f; cb.direction[1] = 0.0f;
	immediateContext->UpdateSubresource(bloomCB.Get(), 0, 0, &cb, 0, 0);
	immediateContext->PSSetShader(blurPS.Get(), nullptr, 0);
	{ ID3D11ShaderResourceView* s = bloomA_SRV.Get(); immediateContext->PSSetShaderResources(0, 1, &s); }
	immediateContext->Draw(3, 0);

	// Blur vertical: bloomB -> bloomA
	immediateContext->PSSetShaderResources(0, 1, &nullSrv);
	immediateContext->OMSetRenderTargets(1, bloomA_RTV.GetAddressOf(), nullptr);
	cb.direction[0] = 0.0f; cb.direction[1] = 1.0f;
	immediateContext->UpdateSubresource(bloomCB.Get(), 0, 0, &cb, 0, 0);
	{ ID3D11ShaderResourceView* s = bloomB_SRV.Get(); immediateContext->PSSetShaderResources(0, 1, &s); }
	immediateContext->Draw(3, 0);

	immediateContext->PSSetShaderResources(0, 1, &nullSrv);
}

void Graphics::BeginScene(float r, float g, float b, float a)
{
	EnsureSceneTarget();
	InitPostProcess();

	if (!sceneRTV)
	{
		Clear(r, g, b, a);
		SetRenderTargets();
		return;
	}

	float color[4] = { r, g, b, a };
	immediateContext->ClearRenderTargetView(sceneRTV.Get(), color);
	immediateContext->ClearDepthStencilView(depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	immediateContext->RSSetViewports(1, &viewport);
	immediateContext->OMSetRenderTargets(1, sceneRTV.GetAddressOf(), depthStencilView.Get());
}

void Graphics::ApplyPostProcess()
{
	if (!sceneRTV || !postVS || !postPS) return;

	if (postBloomEnable)
	{
		RenderBloom();
	}

	PostCB cb{};
	cb.exposure = postExposure;
	cb.contrast = postContrast;
	cb.saturation = postSaturation;
	cb.tonemapEnable = postTonemap ? 1.0f : 0.0f;
	cb.bloomIntensity = postBloomEnable ? postBloomIntensity : 0.0f;
	cb.vignette = postVignette;
	cb.vignetteRadius = postVignetteRadius;
	cb.tint[0] = postTint[0];
	cb.tint[1] = postTint[1];
	cb.tint[2] = postTint[2];
	cb.tint[3] = 1.0f;
	immediateContext->UpdateSubresource(postCB.Get(), 0, 0, &cb, 0, 0);

	immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
	immediateContext->RSSetViewports(1, &viewport);

	immediateContext->IASetInputLayout(nullptr);
	ID3D11Buffer* nullVB = nullptr; UINT zero = 0;
	immediateContext->IASetVertexBuffers(0, 1, &nullVB, &zero, &zero);
	immediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	immediateContext->VSSetShader(postVS.Get(), nullptr, 0);
	immediateContext->PSSetShader(postPS.Get(), nullptr, 0);
	immediateContext->PSSetConstantBuffers(0, 1, postCB.GetAddressOf());

	ID3D11ShaderResourceView* srvs[2] = { sceneSRV.Get(), bloomA_SRV.Get() };
	immediateContext->PSSetShaderResources(0, 2, srvs);
	ID3D11SamplerState* samp = renderState->GetSamplerState(SamplerState::LinearClamp);
	immediateContext->PSSetSamplers(0, 1, &samp);

	immediateContext->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	immediateContext->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	immediateContext->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	immediateContext->Draw(3, 0);

	ID3D11ShaderResourceView* nullSrvs[2] = { nullptr, nullptr };
	immediateContext->PSSetShaderResources(0, 2, nullSrvs);
}