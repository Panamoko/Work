#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "Graphics/RenderState.h"
#include "Graphics/Renderer/PrimitiveRenderer.h"
#include "Graphics/Renderer/ShapeRenderer.h"
#include "Graphics/Model/ModelRenderer.h"

class Graphics
{
private:
	Graphics() = default;
	~Graphics() = default;

public:
	static Graphics& Instance()
	{
		static Graphics instance;
		return instance;
	}

	void Initialize(HWND hWnd);
	void Clear(float r, float g, float b, float a);
	void Resize(UINT width, UINT height);
	void SetRenderTargets();
	void Present(UINT syncInterval);

	HWND                 GetWindowHandle() { return hWnd; }
	ID3D11Device* GetDevice() { return device.Get(); }
	ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }
	float                GetScreenWidth()  const { return screenWidth; }
	float                GetScreenHeight() const { return screenHeight; }

	RenderState* GetRenderState() { return renderState.get(); }
	PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
	ShapeRenderer* GetShapeRenderer()     const { return shapeRenderer.get(); }
	ModelRenderer* GetModelRenderer()     const { return modelRenderer.get(); }

private:
	HWND                                            hWnd = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Device>            device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>     immediateContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>          swapchain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;

	D3D11_VIEWPORT viewport = {};
	float screenWidth = 0;
	float screenHeight = 0;

	std::unique_ptr<RenderState>        renderState;
	std::unique_ptr<PrimitiveRenderer>  primitiveRenderer;
	std::unique_ptr<ShapeRenderer>      shapeRenderer;
	std::unique_ptr<ModelRenderer>      modelRenderer;

	// IBLテクスチャをここで保持しておく（ModelRendererより長生きさせるため）
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradianceSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefilteredSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLutSRV;
};