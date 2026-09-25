#pragma once
#include <DirectXMath.h>


struct ID3D11Device;
struct ID3D11DeviceContext;
namespace uel {
    class World; 
    class MeshShader; 
    class Mesh; 
    class MeshCache; 
    class EngineShadowMap;
    class SkinnedMeshShader;   
    class SkinnedModelCache;  

    class RenderSystem {
    public:
        static void Render(World& world, MeshShader& shader, const Mesh& fallbackMesh,
            MeshCache& cache, ID3D11Device* device,
            ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj);
        static void RenderShadow(World& world, EngineShadowMap& shadow, const Mesh& fallbackMesh,
            MeshCache& cache, ID3D11Device* device, ID3D11DeviceContext* context);
        static void RenderSkinned(World& world, SkinnedMeshShader& shader, SkinnedModelCache& cache,
            ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj);   // Åöí«â¡



    };


} // namespace uel