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

        // 別ファイルのアニメを、書かれた順に足していく
        for (const std::string& file : SplitAnimPaths(animPath)) {
            const size_t before = cpu.clips.size();
            LoadAnimationsFromFile(file, cpu);

            // すでに同じ名前のクリップがあれば、後から来たほうを捨てる。
            // グラフはクリップを名前で探すので、同じ名前が2つあっても先のものしか使われない（T-Pose など）。
            for (size_t i = before; i < cpu.clips.size(); ) {
                bool dup = false;
                for (size_t k = 0; k < i; ++k) if (cpu.clips[k].name == cpu.clips[i].name) { dup = true; break; }
                if (dup) cpu.clips.erase(cpu.clips.begin() + i);
                else ++i;
            }

            const int added = (int)(cpu.clips.size() - before);
            cpu.clipsFromAnimFiles.push_back(added);    // ファイルごとの数（Inspector の表示用）
            cpu.clipsFromAnimFile += added;             // 合計
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