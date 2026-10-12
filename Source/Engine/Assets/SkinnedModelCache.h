#pragma once
#include "Engine/Graphics/SkinnedRenderModel.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

struct ID3D11Device;

namespace uel {

    // animPath には、アニメだけのファイルを「;」で区切って複数書ける。
    //   例： "Data/.../Rig_Medium_MovementBasic.glb;Data/.../Rig_Medium_General.glb"
    // 1つずつに分けて返す（前後の空白と、空の要素は捨てる）。
    inline std::vector<std::string> SplitAnimPaths(const std::string& animPath) {
        std::vector<std::string> out;
        size_t start = 0;
        while (start <= animPath.size()) {
            size_t end = animPath.find(';', start);
            if (end == std::string::npos) end = animPath.size();
            size_t a = start, b = end;
            while (a < b && animPath[a] == ' ') ++a;
            while (b > a && animPath[b - 1] == ' ') --b;
            if (b > a) out.push_back(animPath.substr(a, b - a));
            start = end + 1;
        }
        return out;
    }

    // SplitAnimPaths の逆（「;」でつなぐ）
    inline std::string JoinAnimPaths(const std::vector<std::string>& files) {
        std::string s;
        for (const auto& f : files) { if (!s.empty()) s += ';'; s += f; }
        return s;
    }

    class SkinnedModelCache {
    public:
        std::shared_ptr<SkinnedRenderModel> GetOrLoad(ID3D11Device* device, const std::string& meshPath, const std::string& animPath = "");
    private:
        std::unordered_map<std::string, std::shared_ptr<SkinnedRenderModel>> map;
    };

    // どこからでも使える共有キャッシュ（Inspectorと描画側の両方が参照）
    SkinnedModelCache& GlobalSkinnedCache();

} // namespace uel