#pragma once
#include "Engine/Graphics/SkinnedRenderModel.h"
#include <memory>
#include <string>
#include <unordered_map>

struct ID3D11Device;

namespace uel {

    class SkinnedModelCache {
    public:
        std::shared_ptr<SkinnedRenderModel> GetOrLoad(ID3D11Device* device, const std::string& meshPath, const std::string& animPath = "");
    private:
        std::unordered_map<std::string, std::shared_ptr<SkinnedRenderModel>> map;
    };

    // どこからでも使える共有キャッシュ（Inspectorと描画側の両方が参照）
    SkinnedModelCache& GlobalSkinnedCache();

} // namespace uel