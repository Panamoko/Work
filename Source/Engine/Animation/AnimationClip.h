#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>

namespace uel {

    template<class T> struct Key { float time; T value; };

    struct BoneTrack {
        int bone = -1;                                     // skeleton.bones のindex
        std::vector<Key<DirectX::XMFLOAT3>> pos;
        std::vector<Key<DirectX::XMFLOAT4>> rot;           // クォータニオン
        std::vector<Key<DirectX::XMFLOAT3>> scl;
    };

    struct AnimationClip {
        std::string name;
        float duration = 0.0f;
        std::vector<BoneTrack> tracks;
    };

} // namespace uel