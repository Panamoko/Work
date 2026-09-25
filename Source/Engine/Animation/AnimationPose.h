#pragma once
#include "Engine/Animation/Skeleton.h"
#include "Engine/Animation/AnimationClip.h"
#include <DirectXMath.h>
#include <vector>
#include <functional>

namespace uel {

    inline DirectX::XMFLOAT3 SampleVec3(const std::vector<Key<DirectX::XMFLOAT3>>& keys, float t, DirectX::XMFLOAT3 fb) {
        using namespace DirectX;
        if (keys.empty()) return fb;
        if (t <= keys.front().time) return keys.front().value;
        if (t >= keys.back().time)  return keys.back().value;
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (t < keys[i + 1].time) {
                float a = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
                XMVECTOR v = XMVectorLerp(XMLoadFloat3(&keys[i].value), XMLoadFloat3(&keys[i + 1].value), a);
                XMFLOAT3 r; XMStoreFloat3(&r, v); return r;
            }
        }
        return keys.back().value;
    }

    inline DirectX::XMFLOAT4 SampleQuat(const std::vector<Key<DirectX::XMFLOAT4>>& keys, float t, DirectX::XMFLOAT4 fb) {
        using namespace DirectX;
        if (keys.empty()) return fb;
        if (t <= keys.front().time) return keys.front().value;
        if (t >= keys.back().time)  return keys.back().value;
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (t < keys[i + 1].time) {
                float a = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
                XMVECTOR q = XMQuaternionSlerp(XMLoadFloat4(&keys[i].value), XMLoadFloat4(&keys[i + 1].value), a);
                q = XMQuaternionNormalize(q);
                XMFLOAT4 r; XMStoreFloat4(&r, q); return r;
            }
        }
        return keys.back().value;
    }

    // clip==nullptr なら bind ポーズ。palette[i] = inverseBind[i] * globalPose[i]（行ベクトル形式）
    inline void EvaluatePalette(const Skeleton& skel, const AnimationClip* clip,
        float time, std::vector<DirectX::XMFLOAT4X4>& palette)
    {
        using namespace DirectX;
        const int n = (int)skel.bones.size();
        palette.assign(n, XMFLOAT4X4());
        if (n == 0) return;

        std::vector<int> boneToTrack(n, -1);
        if (clip) for (int ti = 0; ti < (int)clip->tracks.size(); ++ti) {
            int b = clip->tracks[ti].bone;
            if (b >= 0 && b < n) boneToTrack[b] = ti;
        }

        std::vector<XMMATRIX> local(n), global(n);
        std::vector<char> done(n, 0);

        for (int i = 0; i < n; ++i) {
            XMFLOAT3 t = skel.bones[i].t; XMFLOAT4 r = skel.bones[i].r; XMFLOAT3 s = skel.bones[i].s;
            if (clip && boneToTrack[i] >= 0) {
                const BoneTrack& tr = clip->tracks[boneToTrack[i]];
                t = SampleVec3(tr.pos, time, t);
                r = SampleQuat(tr.rot, time, r);
                s = SampleVec3(tr.scl, time, s);
            }
            local[i] = XMMatrixScalingFromVector(XMLoadFloat3(&s))
                * XMMatrixRotationQuaternion(XMLoadFloat4(&r))
                * XMMatrixTranslationFromVector(XMLoadFloat3(&t));
        }

        std::function<XMMATRIX(int)> resolve = [&](int i) -> XMMATRIX {
            if (done[i]) return global[i];
            int p = skel.bones[i].parent;
            global[i] = (p >= 0 && p < n) ? local[i] * resolve(p) : local[i];
            done[i] = 1;
            return global[i];
            };
        for (int i = 0; i < n; ++i) resolve(i);

        for (int i = 0; i < n; ++i) {
            XMMATRIX P = XMLoadFloat4x4(&skel.bones[i].inverseBind) * global[i];
            XMStoreFloat4x4(&palette[i], P);
        }
    }

} // namespace uel