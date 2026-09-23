#include "Engine/Assets/MeshCache.h"
#include "Engine/Assets/GltfMeshLoader.h"
#include "Engine/Graphics/Mesh.h"

namespace uel {

    std::shared_ptr<Mesh> MeshCache::GetOrLoad(ID3D11Device* device, const std::string& path)
    {
        auto it = cache.find(path);
        if (it != cache.end()) return it->second;   // null ‚àŠÜ‚ßƒLƒƒƒbƒVƒ…
        std::shared_ptr<Mesh> m = LoadGltfMesh(device, path);
        cache[path] = m;
        return m;
    }
    std::shared_ptr<Mesh> MeshCache::GetCube(ID3D11Device* device)
    {
        if (!cube) cube = Mesh::CreateCube(device);
        return cube;
    }
} // namespace uel