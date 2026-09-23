#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace uel {

    struct MeshVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 uv;
    };

    class Mesh {
    public:
        struct Submesh {
            UINT indexOffset = 0;
            UINT indexCount = 0;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture; // null なら白
            std::string name;
            bool visible = true;
        };

        bool Create(ID3D11Device* device,
            const MeshVertex* vertices, size_t vertexCount,
            const uint32_t* indices, size_t indexCount,
            const std::vector<Submesh>& subs);

        void Bind(ID3D11DeviceContext* context) const;   // VB/IB/topology
        bool IsValid() const { return !submeshes.empty(); }
        const std::vector<Submesh>& GetSubmeshes() const { return submeshes; }

        const std::vector<DirectX::XMFLOAT3>& Positions() const { return cpuPositions; }
        const std::vector<uint32_t>& Indices()   const { return cpuIndices; }

        static std::shared_ptr<Mesh> CreateCube(ID3D11Device* device);

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
        Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
        std::vector<Submesh> submeshes;
        std::vector<DirectX::XMFLOAT3> cpuPositions;   // ★レイキャスト用
        std::vector<uint32_t>          cpuIndices;     // ★
    };

} // namespace uel