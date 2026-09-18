#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include <cstdint>

namespace uel {

    struct MeshVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };

    class Mesh {
    public:
        bool Create(ID3D11Device* device,
            const MeshVertex* vertices, size_t vertexCount,
            const uint32_t* indices, size_t indexCount);

        void Bind(ID3D11DeviceContext* context) const;
        UINT IndexCount() const { return indexCount; }
        bool IsValid() const { return indexCount > 0; }

        static std::shared_ptr<Mesh> CreateCube(ID3D11Device* device);

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
        Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
        UINT indexCount = 0;
    };

} // namespace uel