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

	void BeginScene(float r, float g, float b, float a);
	void ApplyPostProcess();

	HWND                 GetWindowHandle() { return hWnd; }
	ID3D11Device* GetDevice() { return device.Get(); }
	ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }
	float                GetScreenWidth()  const { return screenWidth; }
	float                GetScreenHeight() const { return screenHeight; }

	RenderState* GetRenderState() { return renderState.get(); }
	PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
	ShapeRenderer* GetShapeRenderer()     const { return shapeRenderer.get(); }
	ModelRenderer* GetModelRenderer()     const { return modelRenderer.get(); }

	// ---- Post-process params (tunable via GUI) ----
	float postExposure = 1.0f;
	float postContrast = 1.0f;
	float postSaturation = 1.0f;
	bool  postTonemap = true;

	bool  postBloomEnable = true;
	float postBloomThreshold = 1.0f;
	float postBloomIntensity = 0.6f;

	// grading
	float postVignette = 0.35f; // edge darkening intensity (0..1)
	float postVignetteRadius = 0.6f;  // where the vignette starts (0..1)
	float postTint[3] = { 1.0f, 1.0f, 1.0f }; // color filter (multiply)

private:
	void EnsureSceneTarget();
	void InitPostProcess();
	void RenderBloom();

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

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblIrradianceSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblPrefilteredSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblBrdfLutSRV;

	// scene target
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   sceneRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneSRV;
	UINT sceneTargetWidth = 0;
	UINT sceneTargetHeight = 0;

	// post + bloom shaders
	Microsoft::WRL::ComPtr<ID3D11VertexShader> postVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  postPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       postCB;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> bloomVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  brightPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  blurPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       bloomCB;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   bloomA_RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bloomA_SRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   bloomB_RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bloomB_SRV;
	UINT bloomWidth = 0;
	UINT bloomHeight = 0;
};