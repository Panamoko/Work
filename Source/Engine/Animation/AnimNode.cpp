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
        const AnimationClip* clip = FindClip(ctx.model);
        float rate = speed;
        if (!speedParam.empty()) rate *= ctx.params.GetFloat(speedParam, 1.0f);
        const float step = ctx.dt * rate;

        // 進める前の位置。頭から始めたばかりのときは「0 より手前」として扱い、時刻 0 の通知も拾えるようにする
        const float prev = fresh ? -1.0f : time;
        if (AdvanceClipTime(time, clip, step, loop)) finished = true;

        // 通知：このフレームに通り過ぎた時刻（prev より後、time 以下）のものを発火する。前へ進んでいるときだけ
        if (ctx.events && clip && step > 0.0f) {
            fresh = false;
            const bool wrapped = loop && time < prev;       // ループして頭に戻った：終わり側と頭側の両方を見る
            for (const Notify& n : notifies) {
                const bool passed = wrapped ? (n.time > prev || n.time <= time) : (n.time > prev && n.time <= time);
                if (passed) ctx.events->push_back(n.name);
            }
        }
    }

    void AnimClipNode::Evaluate(AnimContext& ctx, LocalPose& out)
    {
        // クリップが見つからなければ bind ポーズになる
        SampleLocalPose(ctx.model.skeleton, FindClip(ctx.model), time, out);
    }

} // namespace uel