#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Graphics/CubeRenderer.h"

namespace uel {

    void RenderSystem::Render(World& world, CubeRenderer& cube,
        ID3D11DeviceContext* context,
        const DirectX::XMFLOAT4X4& viewProj)
    {
        for (const auto& slot : world.Slots())
        {
            if (!slot) continue;
            Entity* e = slot.get();

            auto* t = e->GetComponent<TransformComponent>();
            auto* m = e->GetComponent<MeshRendererComponent>();
            if (!t || !m || !m->visible) continue;  // •K—vComponent‚ðŽ‚Â‚à‚Ì‚¾‚¯ˆ—

            cube.Draw(context, t->GetWorldMatrix(), viewProj, m->color);
        }
    }

} // namespace uel