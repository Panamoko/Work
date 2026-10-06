#pragma once
#include "Engine/Animation/Skeleton.h"
#include "Engine/Animation/AnimationClip.h"
#include <DirectXMath.h>
#include <vector>

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

    // ボーン1本ぶんのローカル姿勢。ブレンドはこの形のまま混ぜる（行列にしてからでは混ぜられない）。
    struct BoneTRS {
        DirectX::XMFLOAT3 t{ 0,0,0 };
        DirectX::XMFLOAT4 r{ 0,0,0,1 };
        DirectX::XMFLOAT3 s{ 1,1,1 };
    };
    using LocalPose = std::vector<BoneTRS>;     // skeleton.bones と同じ並び

    // 1) クリップを時刻 time でサンプルして、ローカル姿勢を作る。
    //    clip == nullptr、またはトラックの無いボーンは bind ポーズのまま。
    inline void SampleLocalPose(const Skeleton& skel, const AnimationClip* clip, float time, LocalPose& pose)
    {
        const int n = (int)skel.bones.size();
        pose.resize(n);
        for (int i = 0; i < n; ++i) {
            pose[i].t = skel.bones[i].t; pose[i].r = skel.bones[i].r; pose[i].s = skel.bones[i].s;
        }
        if (!clip) return;
        for (const BoneTrack& tr : clip->tracks) {
            if (tr.bone < 0 || tr.bone >= n) continue;
            BoneTRS& b = pose[tr.bone];
            b.t = SampleVec3(tr.pos, time, b.t);
            b.r = SampleQuat(tr.rot, time, b.r);
            b.s = SampleVec3(tr.scl, time, b.s);
        }
    }

    // 2) ローカル姿勢を親から順に掛けて、モデル空間の行列にする（行ベクトル形式： 子 = ローカル * 親）。
    //    bones は親が先に並んでいるので、前から1回なめるだけでよい。
    inline void ComputeGlobalPose(const Skeleton& skel, const LocalPose& pose, std::vector<DirectX::XMFLOAT4X4>& global)
    {
        using namespace DirectX;
        const int n = (int)skel.bones.size();
        global.resize(n);
        for (int i = 0; i < n; ++i) {
            XMMATRIX local = XMMatrixScalingFromVector(XMLoadFloat3(&pose[i].s))
                * XMMatrixRotationQuaternion(XMLoadFloat4(&pose[i].r))
                * XMMatrixTranslationFromVector(XMLoadFloat3(&pose[i].t));
            const int p = skel.bones[i].parent;
            XMMATRIX g = (p >= 0 && p < i) ? local * XMLoadFloat4x4(&global[p]) : local;
            XMStoreFloat4x4(&global[i], g);
        }
    }

    // 3) スキニング用のパレットを作る。palette[j] = inverseBind[j] * global[joints[j].bone]
    inline void BuildPalette(const Skeleton& skel, const std::vector<DirectX::XMFLOAT4X4>& global,
        std::vector<DirectX::XMFLOAT4X4>& palette)
    {
        using namespace DirectX;
        const int n = (int)skel.joints.size();
        palette.resize(n);
        for (int j = 0; j < n; ++j) {
            const SkinJoint& sj = skel.joints[j];
            XMMATRIX g = (sj.bone >= 0 && sj.bone < (int)global.size()) ? XMLoadFloat4x4(&global[sj.bone]) : XMMatrixIdentity();
            XMStoreFloat4x4(&palette[j], XMLoadFloat4x4(&sj.inverseBind) * g);
        }
    }

    // 1) 〜 3) をまとめて行う（クリップ1つをそのまま再生するとき用）。
    inline void EvaluatePalette(const Skeleton& skel, const AnimationClip* clip,
        float time, std::vector<DirectX::XMFLOAT4X4>& palette)
    {
        LocalPose pose;
        std::vector<DirectX::XMFLOAT4X4> global;
        SampleLocalPose(skel, clip, time, pose);
        ComputeGlobalPose(skel, pose, global);
        BuildPalette(skel, global, palette);
    }

    // a のポーズから b のポーズへ、alpha（0 = a、1 = b）で混ぜる。out は a や b と同じ変数でもよい。
// 位置とスケールは線形補間、回転は球面線形補間（近い側を回る）。
    inline void BlendPose(const LocalPose& a, const LocalPose& b, float alpha, LocalPose& out)
    {
        using namespace DirectX;
        const size_t n = (a.size() < b.size()) ? a.size() : b.size();
        out.resize(n);
        for (size_t i = 0; i < n; ++i) {
            XMVECTOR t = XMVectorLerp(XMLoadFloat3(&a[i].t), XMLoadFloat3(&b[i].t), alpha);
            XMVECTOR r = XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&a[i].r), XMLoadFloat4(&b[i].r), alpha));
            XMVECTOR s = XMVectorLerp(XMLoadFloat3(&a[i].s), XMLoadFloat3(&b[i].s), alpha);
            XMStoreFloat3(&out[i].t, t); XMStoreFloat4(&out[i].r, r); XMStoreFloat3(&out[i].s, s);
        }
    }


} // namespace uel