#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/EngineShadowMap.h"
#include "Engine/Graphics/SkinnedMeshShader.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Components/ShaderComponent.h"

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

            const Mesh* meshToDraw = &fallbackMesh;   // assetId         Η     
            if (!m->assetId.empty()) {
                std::shared_ptr<Mesh> gm = cache.GetOrLoad(device, m->assetId);
                if (gm && gm->IsValid()) meshToDraw = gm.get();
            }
            const ShaderComponent* sc = e->GetComponent<ShaderComponent>();
            const ShadingParams shading = sc ? sc->params : ShadingParams{};
            shader.Draw(context, *meshToDraw, t->GetWorldMatrix(), viewProj, m->color, shading,
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

    namespace {
        // AnimationSystem    v Z     p        ΂    g   B
        //  ܂      Ƃ  i W   C   g ̐       Ȃ  Ƃ  j     A   ̏ Ōv Z    B
        const std::vector<DirectX::XMFLOAT4X4>& PaletteOf(const SkinnedMeshComponent& sk, const SkinnedModel& cpu,
            std::vector<DirectX::XMFLOAT4X4>& fallback)
        {
            if (!sk.palette.empty() && sk.palette.size() == cpu.skeleton.joints.size()) return sk.palette;
            const AnimationClip* clip =
                (sk.clipIndex >= 0 && sk.clipIndex < (int)cpu.clips.size()) ? &cpu.clips[sk.clipIndex] : nullptr;
            EvaluatePalette(cpu.skeleton, clip, sk.time, fallback);
            return fallback;
        }
    }

    void RenderSystem::RenderSkinned(World& world, SkinnedMeshShader& shader, SkinnedModelCache& cache,
        ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj)
    {
        std::vector<DirectX::XMFLOAT4X4> fallback;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* sk = e->GetComponent<SkinnedMeshComponent>();
            if (!t || !sk || !sk->visible || sk->assetPath.empty()) continue;

            auto model = cache.GetOrLoad(device, sk->assetPath, sk->animPath);
            if (!model || !model->Valid()) continue;

            const auto& palette = PaletteOf(*sk, model->Cpu(), fallback);
            const ShaderComponent* sc = e->GetComponent<ShaderComponent>();
            const ShadingParams shading = sc ? sc->params : ShadingParams{};
            shader.Draw(context, *model, t->GetWorldMatrix(), viewProj, sk->color, shading, palette);
        }
    }

    void RenderSystem::RenderShadowSkinned(World& world, EngineShadowMap& shadow, SkinnedModelCache& cache,
        ID3D11Device* device, ID3D11DeviceContext* context)
    {
        std::vector<DirectX::XMFLOAT4X4> fallback;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            auto* sk = e->GetComponent<SkinnedMeshComponent>();
            if (!t || !sk || !sk->visible || sk->assetPath.empty()) continue;

            auto model = cache.GetOrLoad(device, sk->assetPath, sk->animPath);
            if (!model || !model->Valid()) continue;

            //  { `  Ɠ    p   ŉe 𗎂Ƃ  i v Z ͂  Ȃ  BAnimationSystem  ̌  ʂ ǂނ    j
            shadow.DrawSkinned(context, *model, t->GetWorldMatrix(), PaletteOf(*sk, model->Cpu(), fallback));
        }
    }


} // namespace uel