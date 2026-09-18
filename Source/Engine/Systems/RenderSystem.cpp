#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Assets/MeshCache.h"

namespace uel {

    void RenderSystem::Render(World& world, MeshShader& shader, const Mesh& fallbackMesh,
        MeshCache& cache, ID3D11Device* device,
        ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj)
    {
        for (const auto& slot : world.Slots())
        {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* m = e->GetComponent<MeshRendererComponent>();
            if (!t || !m || !m->visible) continue;

            const Mesh* meshToDraw = &fallbackMesh;   // assetId ‚ª–³‚¯‚ê‚Î—§•û‘Ì
            if (!m->assetId.empty()) {
                std::shared_ptr<Mesh> gm = cache.GetOrLoad(device, m->assetId);
                if (gm && gm->IsValid()) meshToDraw = gm.get();
            }
            shader.Draw(context, *meshToDraw, t->GetWorldMatrix(), viewProj, m->color);
        }
    }

} // namespace uel