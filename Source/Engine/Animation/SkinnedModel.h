#pragma once
#include "Engine/Animation/Skeleton.h"
#include "Engine/Animation/AnimationClip.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <cstdint>

namespace uel {

    struct SkinnedVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 uv;
        uint32_t bones[4];      // skeleton.bones への index
        float    weights[4];
    };


    struct SkinnedTexture { int width = 0, height = 0; std::vector<uint8_t> rgba; };
    struct SkinnedSubmesh { uint32_t indexOffset = 0, indexCount = 0; std::string name; int texture = -1; };

    struct SkinnedModel {
        Skeleton skeleton;
        std::vector<AnimationClip>  clips;
        std::vector<SkinnedVertex>  vertices;
        std::vector<uint32_t>       indices;
        std::vector<SkinnedSubmesh> submeshes;
        std::vector<SkinnedTexture> textures;   // ②-2でSRV化
        int clipsFromAnimFile = 0;              // Anim GLB（別ファイル）から追加されたクリップの数
        std::vector<int> clipsFromAnimFiles;    // Anim GLB のファイルごとに、追加されたクリップの数
        bool Valid() const { return !vertices.empty() && !indices.empty(); }
    };

} // namespace uel