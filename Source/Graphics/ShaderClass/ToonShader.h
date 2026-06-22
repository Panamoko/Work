#pragma once

#include <wrl.h>
#include <d3d11.h>
#include "Graphics/ShaderClass/Shader.h"

// =======================================================
// 本体描画用 トゥーンシェーダー（NPR-PBR）
// =======================================================
class ToonShader : public Shader
{
public:
    ToonShader(ID3D11Device* device);
    ~ToonShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColorFactor;
        DirectX::XMFLOAT3 emissiveFactor;
        float             metalnessFactor;
        float             roughnessFactor;
        float             occlusionStrength;
        float             alphaCutoff;
        int               alphaMode;
        int               hasBaseColorMap;
        int               hasNormalMap;
        int               hasMetalnessRoughnessMap;
        int               hasEmissiveMap;
        int               hasOcclusionMap;
        int               _pad0;
        int               _pad1;
        int               _pad2;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> meshConstantBuffer;

    // ★修正：シェーダーオブジェクトの保持変数を追加
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
};

// =======================================================
// 輪郭線（アウトライン）描画用シェーダー
// =======================================================
class ToonOutlineShader : public Shader
{
public:
    ToonOutlineShader(ID3D11Device* device);
    ~ToonOutlineShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    struct CbOutline
    {
        float outlineWidth;         // 輪郭線の太さ
        DirectX::XMFLOAT3 pad;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> outlineConstantBuffer;

    // ★修正：シェーダーオブジェクトの保持変数を追加
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
};