#include "Engine/Graphics/SkinnedRenderModel.h"

using Microsoft::WRL::ComPtr;

namespace uel {

    bool SkinnedRenderModel::Upload(ID3D11Device* device, const SkinnedModel& src)
    {
        if (!device || !src.Valid()) return false;
        cpu = src;
        vb.Reset(); ib.Reset(); texSRVs.clear(); indexCount = 0;

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.ByteWidth = (UINT)(cpu.vertices.size() * sizeof(SkinnedVertex));
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd = {}; vsd.pSysMem = cpu.vertices.data();
        if (FAILED(device->CreateBuffer(&vbd, &vsd, vb.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.ByteWidth = (UINT)(cpu.indices.size() * sizeof(uint32_t));
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd = {}; isd.pSysMem = cpu.indices.data();
        if (FAILED(device->CreateBuffer(&ibd, &isd, ib.GetAddressOf()))) return false;
        indexCount = (UINT)cpu.indices.size();

        texSRVs.resize(cpu.textures.size());
        for (size_t i = 0; i < cpu.textures.size(); ++i) {
            const auto& t = cpu.textures[i];
            if (t.width <= 0 || t.height <= 0 || t.rgba.empty()) continue;
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = t.width; td.Height = t.height; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd = {}; sd.pSysMem = t.rgba.data(); sd.SysMemPitch = t.width * 4;
            ComPtr<ID3D11Texture2D> tex;
            if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
                device->CreateShaderResourceView(tex.Get(), nullptr, texSRVs[i].GetAddressOf());
        }
        return true;
    }

    void SkinnedRenderModel::Bind(ID3D11DeviceContext* ctx) const
    {
        UINT stride = sizeof(SkinnedVertex), offset = 0;
        ctx->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
        ctx->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    ID3D11ShaderResourceView* SkinnedRenderModel::TexSRV(int texIndex) const
    {
        if (texIndex < 0 || texIndex >= (int)texSRVs.size()) return nullptr;
        return texSRVs[texIndex].Get();
    }

} // namespace uel