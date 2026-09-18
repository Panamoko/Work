#pragma once
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Scene/EntityId.h"
#include "Engine/Assets/MeshCache.h"
#include <DirectXMath.h>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace uel {
    class World;

    class ViewportPanel {
    public:
        void Initialize(ID3D11Device* device);
        void DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history);
        void RenderScene(ID3D11DeviceContext* context, World& world);

    private:
        RenderTexture rt;
        MeshShader    shader;
        std::shared_ptr<Mesh> cubeMesh;
        int viewWidth = 16;
        int viewHeight = 16;

        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 proj{};
        int gizmoOp = 0;
        TransformEditSession gizmoSession;
        MeshCache      meshCache;
        ID3D11Device* devicePtr = nullptr;
    };

} // namespace uel