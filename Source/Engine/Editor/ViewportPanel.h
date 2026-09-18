#pragma once
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/CubeRenderer.h"
#include "Engine/Editor/TransformEdit.h"   // TransformEditSession / CommandHistory
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace uel {
    class World;

    class ViewportPanel {
    public:
        void Initialize(ID3D11Device* device);
        void DrawUI(ID3D11Device* device, World& world, EntityId selected, CommandHistory& history);
        void RenderScene(ID3D11DeviceContext* context, World& world);

    private:
        RenderTexture rt;
        CubeRenderer  cube;
        int viewWidth = 16;
        int viewHeight = 16;

        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 proj{};
        int gizmoOp = 0;

        TransformEditSession gizmoSession;   // ギズモ専用の編集セッション
    };

} // namespace uel