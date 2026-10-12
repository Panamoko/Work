#include "Engine/Animation/AnimNode.h"

namespace uel {

    const AnimationClip* AnimClipNode::FindClip(const SkinnedModel& model)
    {
        if (cachedModel != &model || cachedName != clipName) {
            cachedModel = &model;
            cachedName = clipName;
            cachedIndex = -1;
            for (int i = 0; i < (int)model.clips.size(); ++i)
                if (model.clips[i].name == clipName) { cachedIndex = i; break; }
        }
        return (cachedIndex >= 0 && cachedIndex < (int)model.clips.size()) ? &model.clips[cachedIndex] : nullptr;
    }

    void AnimClipNode::Update(AnimContext& ctx)
    {
        float rate = speed;
        if (!speedParam.empty()) rate *= ctx.params.GetFloat(speedParam, 1.0f);
        if (AdvanceClipTime(time, FindClip(ctx.model), ctx.dt * rate, loop)) finished = true;
    }

    void AnimClipNode::Evaluate(AnimContext& ctx, LocalPose& out)
    {
        // クリップが見つからなければ bind ポーズになる
        SampleLocalPose(ctx.model.skeleton, FindClip(ctx.model), time, out);
    }

} // namespace uel