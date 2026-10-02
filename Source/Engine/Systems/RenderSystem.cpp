#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/EngineShadowMap.h"
#include "Engine/Graphics/SkinnedMeshShader.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Animation/AnimationPose.h"
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
            shader.Draw(context, *meshToDraw, t->GetWorldMatrix(), viewProj, m->color,
                &m->hiddenSubmesh, &m->submeshRole, m->firstPerson);
        }
    }
    void RenderSystem::RenderShadow(World& world, EngineShadowMap& shadow, const Mesh& fallbackMesh,
        MeshCache& cache, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* m = e->GetComponent<MeshRendererComponent>();
            if (!t || !m || !m->visible) continue;
            const Mesh* mesh = &fallbackMesh;
            if (!m->assetId.empty()) { auto sp = cache.GetOrLoad(device, m->assetId); if (sp && sp->IsValid()) mesh = sp.get(); }
            shadow.DrawMesh(context, *mesh, t->GetWorldMatrix());
        }
    }
    void RenderSystem::RenderSkinned(World& world, SkinnedMeshShader& shader, SkinnedModelCache& cache,
        ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj)
    {
        std::vector<DirectX::XMFLOAT4X4> palette;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* sk = e->GetComponent<SkinnedMeshComponent>();
            if (!t || !sk || !sk->visible || sk->assetPath.empty()) continue;

            auto model = cache.GetOrLoad(device, sk->assetPath, sk->animPath);
            if (!model || !model->Valid()) continue;

            const auto& clips = model->Cpu().clips;
            const AnimationClip* clip =
                (sk->clipIndex >= 0 && sk->clipIndex < (int)clips.size()) ? &clips[sk->clipIndex] : nullptr;

            EvaluatePalette(model->Cpu().skeleton, clip, sk->time, palette);
            shader.Draw(context, *model, t->GetWorldMatrix(), viewProj, sk->color, palette);
        }
    }

    void RenderSystem::RenderShadowSkinned(World& world, EngineShadowMap& shadow, SkinnedModelCache& cache,
        ID3D11Device* device, ID3D11DeviceContext* context)
    {
        std::vector<DirectX::XMFLOAT4X4> palette;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* sk = e->GetComponent<SkinnedMeshComponent>();
            if (!t || !sk || !sk->visible || sk->assetPath.empty()) continue;

            auto model = cache.GetOrLoad(device, sk->assetPath, sk->animPath);
            if (!model || !model->Valid()) continue;

            const auto& clips = model->Cpu().clips;
            const AnimationClip* clip =
                (sk->clipIndex >= 0 && sk->clipIndex < (int)clips.size()) ? &clips[sk->clipIndex] : nullptr;

            EvaluatePalette(model->Cpu().skeleton, clip, sk->time, palette);   // –{•`‰æ‚Æ“¯‚¶Žp¨‚Å‰e‚ð—Ž‚Æ‚·
            shadow.DrawSkinned(context, *model, t->GetWorldMatrix(), palette);
        }
    }

} // namespace uel