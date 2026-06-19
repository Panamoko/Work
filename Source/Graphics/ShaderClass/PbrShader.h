#pragma once

#include "Graphics/ShaderClass/Shader.h"

// 物理ベースレンダリング（PBR）用シェーダー。
// 既存の BasicShader / LambertShader と同じく Shader 基底を継承し、
// Begin / Update / End の3メソッドで描画パスを構成する。
class PbrShader : public Shader
{
public:
    PbrShader(ID3D11Device* device);
    ~PbrShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    // PbrPS.hlsl の cbuffer CbMesh(b0) と「並び・サイズ」を完全一致させること。
    // HLSL の16byteパッキング規則に合わせ、末尾をパディングで埋めている（合計80byte）。
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColorFactor;          // ベースカラー係数
        DirectX::XMFLOAT3 emissiveFactor;           // 自己発光色
        float             metalnessFactor;          // 金属度係数
        float             roughnessFactor;          // 粗さ係数
        float             occlusionStrength;        // AOの効き具合
        float             alphaCutoff;              // MASKしきい値
        int               alphaMode;                // 0=Opaque 1=Mask 2=Blend
        int               hasBaseColorMap;          // テクスチャ有無フラグ群
        int               hasNormalMap;
        int               hasMetalnessRoughnessMap;
        int               hasEmissiveMap;
        int               hasOcclusionMap;
        int               _pad0;                    // 16byteアラインメント用
        int               _pad1;
        int               _pad2;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       meshConstantBuffer;
};