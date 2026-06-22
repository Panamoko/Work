#include "Core/Misc.h"
#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h"

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