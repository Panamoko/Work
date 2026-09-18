#pragma once
#include <DirectXMath.h>

struct ID3D11DeviceContext;

namespace uel {
    class World;
    class CubeRenderer;

    // Transform＋MeshRenderer を持つ Entity のみを描画する（仕様書5.3）。
    class RenderSystem {
    public:
        static void Render(World& world, CubeRenderer& cube,
            ID3D11DeviceContext* context,
            const DirectX::XMFLOAT4X4& viewProj);
    };

} // namespace uel