#pragma once
#include "Engine/Animation/SkinnedModel.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace uel {

    // SkinnedModel(CPU) から GPU 資源（VB/IB/テクスチャSRV）を作って保持。
    class SkinnedRenderModel {
    public:
        bool Upload(ID3D11Device* device, const SkinnedModel& src);
        void Bind(ID3D11DeviceContext* ctx) const;

        const SkinnedModel& Cpu() const { return cpu; }
        const std::vector<SkinnedSubmesh>& Submeshes() const { return cpu.submeshes; }
        ID3D11ShaderResourceView* TexSRV(int texIndex) const;

        bool  Valid()      const { return vb && ib && indexCount > 0; }
        UINT  IndexCount() const { return indexCount; }

    private:
        SkinnedModel cpu;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vb, ib;
        UINT indexCount = 0;
        std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texSRVs;
    };

} // namespace uel