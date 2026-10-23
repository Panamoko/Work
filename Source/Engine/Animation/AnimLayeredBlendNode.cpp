#include "Engine/Animation/AnimLayeredBlendNode.h"

namespace uel {

    void AnimLayeredBlendNode::Update(AnimContext& ctx)
    {
        // 両方とも進める（通知も両方から出る）
        if (basePose)  basePose->Update(ctx);
        if (blendPose) blendPose->Update(ctx);
    }

    void AnimLayeredBlendNode::Reset()
    {
        if (basePose)  basePose->Reset();
        if (blendPose) blendPose->Reset();
    }

    void AnimLayeredBlendNode::UpdateMask(const Skeleton& skel)
    {
        if (maskSkeleton == &skel && maskBuiltBone == maskBone && maskBuiltDepth == maskBlendDepth
            && mask.size() == skel.bones.size()) return;
        maskSkeleton = &skel;
        maskBuiltBone = maskBone;
        maskBuiltDepth = maskBlendDepth;
        if (maskBone.empty()) mask.assign(skel.bones.size(), 1.0f);    // 骨を選んでいなければ全身
        else BuildBoneMask(skel, maskBone, maskBlendDepth, mask);       // 見つからなければ全部 0（重ならない）
    }

    void AnimLayeredBlendNode::Evaluate(AnimContext& ctx, LocalPose& out)
    {
        const Skeleton& skel = ctx.model.skeleton;

        // 1) 下地
        //    out には前の結果（別の Entity の姿勢など）が残っていることがあるので、下地が無いときは必ず作り直す
        if (basePose) basePose->Evaluate(ctx, out);
        if (!basePose || out.size() != skel.bones.size()) SampleLocalPose(skel, nullptr, 0.0f, out);     // 下地が無い・何も出さなかった：bind ポーズ

        // 2) 重み（weight × パラメータ）
        float w = weight;
        if (!weightParam.empty()) {
            float p = ctx.params.GetFloat(weightParam, 1.0f);
            w *= (p < 0.0f) ? 0.0f : (p > 1.0f) ? 1.0f : p;
        }
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        lastWeight = w;
        if (!blendPose || w <= 0.0f) return;

        // 3) 重ねる側を、範囲の骨だけに混ぜる
        blendPose->Evaluate(ctx, scratch);
        if (scratch.size() != out.size()) return;      // 重ねる側が何も出さなかった
        UpdateMask(skel);
        BlendPoseMasked(out, scratch, w, mask, out);
    }

} // namespace uel
