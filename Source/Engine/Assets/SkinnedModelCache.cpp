#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Assets/GltfSkinnedLoader.h"

namespace uel {

    std::shared_ptr<SkinnedRenderModel> SkinnedModelCache::GetOrLoad(ID3D11Device* device, const std::string& meshPath, const std::string& animPath)
    {
        if (meshPath.empty() || !device) return nullptr;
        std::string key = meshPath + "|" + animPath;
        auto it = map.find(key);
        if (it != map.end()) return it->second;

        SkinnedModel cpu;
        if (!LoadSkinnedModel(meshPath, cpu)) { map[key] = nullptr; return nullptr; }
        if (!animPath.empty()) {
            const size_t before = cpu.clips.size();
            LoadAnimationsFromFile(animPath, cpu);
            cpu.clipsFromAnimFile = (int)(cpu.clips.size() - before);
        }
        auto rm = std::make_shared<SkinnedRenderModel>();
        if (!rm->Upload(device, cpu)) { map[key] = nullptr; return nullptr; }
        map[key] = rm;
        return rm;
    }

    SkinnedModelCache& GlobalSkinnedCache()
    {
        static SkinnedModelCache instance;
        return instance;
    }

} // namespace uel