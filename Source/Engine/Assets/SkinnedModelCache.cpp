#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Assets/GltfSkinnedLoader.h"

namespace uel {

    std::shared_ptr<SkinnedRenderModel> SkinnedModelCache::GetOrLoad(ID3D11Device* device, const std::string& path)
    {
        if (path.empty() || !device) return nullptr;
        auto it = map.find(path);
        if (it != map.end()) return it->second;

        SkinnedModel cpu;
        if (!LoadSkinnedModel(path, cpu)) { map[path] = nullptr; return nullptr; }
        auto rm = std::make_shared<SkinnedRenderModel>();
        if (!rm->Upload(device, cpu)) { map[path] = nullptr; return nullptr; }
        map[path] = rm;
        return rm;
    }

    SkinnedModelCache& GlobalSkinnedCache()
    {
        static SkinnedModelCache instance;
        return instance;
    }

} // namespace uel