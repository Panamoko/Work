#pragma once
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Animation/AnimParams.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

namespace uel {

    // エイムオフセット（UE の Aim Offset に当たる。ここでは「骨を回して作る」手続き型）。
    // 視点の上下・左右の角度（パラメータ）に合わせて、背骨の何本かを少しずつ曲げ、上半身を狙う方向へ向ける。
    // 上下を向いたクリップを用意しなくても、どのモデルでも動く（曲げる骨を名前で選ぶだけ）。
    // グラフとモンタージュの計算が終わった後に、AnimationSystem が最後にかける。
    struct AnimAimOffset {
        bool        enabled = false;
        std::string startBone;                  // 曲げ始める骨（腰に近い側。例："spine"）
        std::string endBone;                    // 曲げ終わる骨（頭に近い側。例："chest"）。startBone 自身か、その子孫
        std::string pitchParam = "AimPitch";    // 上下の角度の Float パラメータ（度。上が +）。空なら使わない
        std::string yawParam;                   // 左右の角度の Float パラメータ（度。右が +）。空なら使わない
        float       maxPitch = 60.0f;           // これより大きい角度は切る（度）
        float       maxYaw = 60.0f;
        bool        invertPitch = false;        // モデルの向きによって逆に曲がるときに入れる
        bool        invertYaw = false;
    };

    // 曲げる骨の並び（startBone から endBone まで、親が先）。つながっていなければ空にして false
    inline bool AimChain(const Skeleton& skel, const AnimAimOffset& a, std::vector<int>& chain)
    {
        chain.clear();
        const int start = skel.Find(a.startBone);
        int b = skel.Find(a.endBone);
        if (start < 0 || b < 0) return false;
        while (b >= 0) {                        // endBone から親へたどり、startBone に着いたら終わり
            chain.insert(chain.begin(), b);
            if (b == start) return true;
            b = skel.bones[b].parent;
        }
        chain.clear();                          // startBone の子孫ではなかった
        return false;
    }

    // ローカル姿勢 pose に、エイムオフセットをかける
    inline void ApplyAimOffset(const Skeleton& skel, const AnimAimOffset& a, const AnimParams& params, LocalPose& pose)
    {
        using namespace DirectX;
        if (!a.enabled || pose.size() != skel.bones.size()) return;

        auto clampDeg = [](float v, float lim) { return (v > lim) ? lim : (v < -lim) ? -lim : v; };
        float pitch = a.pitchParam.empty() ? 0.0f : clampDeg(params.GetFloat(a.pitchParam, 0.0f), a.maxPitch);
        float yaw = a.yawParam.empty() ? 0.0f : clampDeg(params.GetFloat(a.yawParam, 0.0f), a.maxYaw);
        if (a.invertPitch) pitch = -pitch;
        if (a.invertYaw) yaw = -yaw;
        if (pitch == 0.0f && yaw == 0.0f) return;

        std::vector<int> chain;
        if (!AimChain(skel, a, chain)) return;

        // 1本あたりの回転（モデルの空間で）。角度を骨の本数で割って、少しずつ曲げる。
        //   上下：X 軸まわり。左手系では + に回すと前（+Z）が下を向くので、上を + にするため符号を逆にする
        //   左右：Y 軸まわり
        const float n = (float)chain.size();
        const XMVECTOR dPitch = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XMConvertToRadians(-pitch / n));
        const XMVECTOR dYaw = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(yaw / n));
        const XMVECTOR delta = XMQuaternionMultiply(dPitch, dYaw);         // 上下を回してから、左右を回す

        // 最初の骨の、親までのモデル空間での回転（根から順に掛けていく。行ベクトル形式：子の回転 → 親の回転）
        std::vector<int> ancestors;
        for (int p = skel.bones[chain[0]].parent; p >= 0; p = skel.bones[p].parent) ancestors.insert(ancestors.begin(), p);
        XMVECTOR parentG = XMQuaternionIdentity();
        for (int p : ancestors) parentG = XMQuaternionMultiply(XMLoadFloat4(&pose[p].r), parentG);

        // 骨ごとに：モデル空間での向き G = ローカル × 親 に delta を足し、ローカルに戻す（ローカル = G × 親の逆）
        for (int b : chain) {
            const XMVECTOR local = XMLoadFloat4(&pose[b].r);
            const XMVECTOR g = XMQuaternionMultiply(XMQuaternionMultiply(local, parentG), delta);
            const XMVECTOR newLocal = XMQuaternionNormalize(XMQuaternionMultiply(g, XMQuaternionInverse(parentG)));
            XMStoreFloat4(&pose[b].r, newLocal);
            parentG = g;                        // 次の骨（この骨の子）の親の向き
        }
    }

} // namespace uel
