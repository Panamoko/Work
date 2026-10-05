#include "Engine/Graphics/MeshShader.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/ShaderCommon.h"
#include <string>
#include <cstdint>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // 静的メッシュ専用の部分（頂点シェーダだけ）。
        // 定数バッファ・影・ピクセルシェーダは ShaderCommon.h の kShaderCommonHLSL にある。
        // ToWorld でワールド位置と法線を求め、通常用とアウトライン用の両方で使う。
        const char* kMeshVS = R"(
struct VSIn { float3 pos:POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD; };
void ToWorld(VSIn i, out float3 wpos, out float3 wnrm) {
    wpos = mul(float4(i.pos, 1.0), world).xyz;
    wnrm = mul(i.nrm, (float3x3)world);
}
PSIn VSMain(VSIn i)    { float3 p; float3 n; ToWorld(i, p, n); return MakePSIn(p, n, i.col, i.uv); }
PSIn VSOutline(VSIn i) { float3 p; float3 n; ToWorld(i, p, n); return MakeOutlinePSIn(p, n, i.col, i.uv); }
)";
    }

    bool MeshShader::Initialize(ID3D11Device* device)
    {
        // 共通部分 ＋ 自分の頂点シェーダ、の順に連結する（PSIn が先に宣言されている必要がある）
        const std::string source = std::string(kShaderCommonHLSL) + kMeshVS;

        ComPtr<ID3DBlob> vsBlob, psBlob;
        if (!CompileShaderSource(source, "mesh", "VSMain", "vs_5_0", vsBlob.GetAddressOf())) return false;
        if (!CompileShaderSource(source, "mesh", "PSMain", "ps_5_0", psBlob.GetAddressOf())) return false;
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

        // アウトライン用（頂点の形は同じなので、入力レイアウトは下の1つを共用する）
        ComPtr<ID3DBlob> vsOutlineBlob, psOutlineBlob;
        if (!CompileShaderSource(source, "mesh", "VSOutline", "vs_5_0", vsOutlineBlob.GetAddressOf())) return false;
        if (!CompileShaderSource(source, "mesh", "PSOutline", "ps_5_0", psOutlineBlob.GetAddressOf())) return false;
        if (FAILED(device->CreateVertexShader(vsOutlineBlob->GetBufferPointer(), vsOutlineBlob->GetBufferSize(), nullptr, vsOutline.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psOutlineBlob->GetBufferPointer(), psOutlineBlob->GetBufferSize(), nullptr, psOutline.GetAddressOf()))) return false;

        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), layout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT; cbd.ByteWidth = sizeof(ObjectCB); cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        // アウトライン用：表面を捨てて裏面だけ描く（時計回りが表。立方体も glTF もこの向き）
        D3D11_RASTERIZER_DESC ord = {};
        ord.FillMode = D3D11_FILL_SOLID; ord.CullMode = D3D11_CULL_FRONT; ord.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&ord, rasterOutline.GetAddressOf()))) return false;

        // 通常のサンプラ（s0）。スキンド側と同じ設定。
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) return false;

        // 影用の比較サンプラ（s1）
        D3D11_SAMPLER_DESC ssd = {};
        ssd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        ssd.AddressU = ssd.AddressV = ssd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        ssd.BorderColor[0] = ssd.BorderColor[1] = ssd.BorderColor[2] = ssd.BorderColor[3] = 1.0f;
        ssd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(device->CreateSamplerState(&ssd, shadowSampler.GetAddressOf()))) return false;

        // 1x1 の白テクスチャ（テクスチャを持たないメッシュ用）
        uint32_t white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC wd = {};
        wd.Width = wd.Height = 1; wd.MipLevels = 1; wd.ArraySize = 1;
        wd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; wd.SampleDesc.Count = 1;
        wd.Usage = D3D11_USAGE_IMMUTABLE; wd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA wsd = {}; wsd.pSysMem = &white; wsd.SysMemPitch = 4;
        ComPtr<ID3D11Texture2D> wtex;
        if (FAILED(device->CreateTexture2D(&wd, &wsd, wtex.GetAddressOf()))) return false;
        if (FAILED(device->CreateShaderResourceView(wtex.Get(), nullptr, whiteSRV.GetAddressOf()))) return false;

        return true;
    }

    void MeshShader::Draw(ID3D11DeviceContext* context, const Mesh& mesh,
        const XMFLOAT4X4& world, const XMFLOAT4X4& viewProj, const XMFLOAT4& color,
        const ShadingParams& shading,
        const std::vector<uint8_t>* hiddenSubmesh,
        const std::vector<uint8_t>* submeshRole,
        bool firstPerson)
    {
        if (!mesh.IsValid()) return;

        ObjectCB data;
        XMStoreFloat4x4(&data.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        XMStoreFloat4x4(&data.viewProj, XMMatrixTranspose(XMLoadFloat4x4(&viewProj)));
        XMStoreFloat4x4(&data.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&lightViewProj)));
        data.color = color;
        data.lightDir = XMFLOAT4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
        data.lightColor = XMFLOAT4(lightColor.x, lightColor.y, lightColor.z, 0.0f);
        data.params = XMFLOAT4(lightAmbient, lightIntensity, shadowEnabled ? 1.0f : 0.0f, 0.0f);
        FillShading(data, shading);
        data.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
        context->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);

        context->IASetInputLayout(layout.Get());
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->PSSetShader(ps.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->RSSetState(raster.Get());
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
        context->PSSetShaderResources(1, 1, &shadowSRV);
        context->PSSetSamplers(1, 1, shadowSampler.GetAddressOf());

        mesh.Bind(context);
        const auto& subs = mesh.GetSubmeshes();

        // 表示するサブメッシュを順に描く。本描画とアウトラインの2回使う。
        auto drawSubmeshes = [&](bool bindTexture) {
            for (size_t i = 0; i < subs.size(); ++i) {
                const auto& sm = subs[i];
                if (!sm.visible || sm.indexCount == 0) continue;

                bool hide = false;
                if (hiddenSubmesh && i < hiddenSubmesh->size() && (*hiddenSubmesh)[i]) hide = true;          // 手動で非表示
                if (firstPerson && submeshRole && i < submeshRole->size()
                    && (*submeshRole)[i] == (uint8_t)1 /*FirstPersonHidden*/) hide = true;                   // 一人称で非表示
                if (hide) continue;

                if (bindTexture) {
                    ID3D11ShaderResourceView* srv = sm.texture ? sm.texture.Get() : whiteSRV.Get();
                    context->PSSetShaderResources(0, 1, &srv);
                }
                context->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
            }
            };

        // 本描画
        drawSubmeshes(true);

        // アウトライン（背面法）：法線方向に少しふくらませた形の「裏面だけ」を単色で描く。
        // 元の形より外にはみ出た部分だけが見えるので、輪郭線になる。
        // 定数バッファは本描画と同じもの（outline の色と太さも入っている）を使う。
        if (shading.outlineWidth > 0.0f) {
            context->VSSetShader(vsOutline.Get(), nullptr, 0);
            context->PSSetShader(psOutline.Get(), nullptr, 0);
            context->RSSetState(rasterOutline.Get());
            drawSubmeshes(false);
        }
    }

    void MeshShader::SetLight(const SceneLighting& light)
    {
        lightDir = light.direction;
        lightColor = light.color;
        lightAmbient = light.ambient;
        lightIntensity = light.intensity;
    }

    void MeshShader::SetShadow(ID3D11ShaderResourceView* srv, const XMFLOAT4X4& lvp)
    {
        shadowSRV = srv;
        lightViewProj = lvp;
        shadowEnabled = (srv != nullptr);
    }

} // namespace uel