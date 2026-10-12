#include "Engine/Animation/AnimBlendSpaceNode.h"
#include <cmath>

namespace uel {
    namespace {
        // サンプルは数個なので、名前から毎回探す（クリップが見つからなければ nullptr）
        const AnimationClip* FindClipByName(const SkinnedModel& model, const std::string& name) {
            for (const auto& c : model.clips) if (c.name == name) return &c;
            return nullptr;
        }
    }

    void AnimBlendSpace1DNode::Update(AnimContext& ctx)
    {
        // 1) 入力の値を、なめらかに追いかける（急に変えても姿勢が飛ばないように）
        const float target = ctx.params.GetFloat(param, 0.0f);
        const float adt = std::fabs(ctx.dt);
        if (!started || smoothTime <= 0.0f) { value = target; started = true; }
        else value += (target - value) * (1.0f - std::exp(-adt / smoothTime));

        // 2) 値をはさむ2つのサンプルを探す（lower：値以下でいちばん近い / upper：値以上でいちばん近い）
        lower = upper = -1;
        for (int i = 0; i < (int)samples.size(); ++i) {
            if (!FindClipByName(ctx.model, samples[i].clipName)) continue;     // このモデルに無いクリップは使わない
            const float p = samples[i].position;
            if (p <= value && (lower < 0 || p > samples[lower].position)) lower = i;
            if (p >= value && (upper < 0 || p < samples[upper].position)) upper = i;
        }
        if (lower < 0) lower = upper;       // 値がいちばん小さいサンプルより下：端のクリップだけ
        if (upper < 0) upper = lower;       // 値がいちばん大きいサンプルより上：端のクリップだけ
        if (lower < 0) { weight = 0.0f; return; }                               // 使えるサンプルが1つも無い

        const float span = samples[upper].position - samples[lower].position;
        weight = (lower == upper || span <= 1e-6f) ? 0.0f : (value - samples[lower].position) / span;

        // 3) 1周の長さを混ぜ具合に合わせて決め、位相を進める
        const float d0 = FindClipByName(ctx.model, samples[lower].clipName)->duration;
        const float d1 = FindClipByName(ctx.model, samples[upper].clipName)->duration;
        const float dur = d0 + (d1 - d0) * weight;
        if (dur > 1e-6f) {
            phase += ctx.dt * speed / dur;
            phase -= std::floor(phase);     // 0〜1 に収める（逆再生でマイナスになっても戻る）
        }
    }

    void AnimBlendSpace1DNode::Evaluate(AnimContext& ctx, LocalPose& out)
    {
        const AnimationClip* c0 = (lower >= 0 && lower < (int)samples.size()) ? FindClipByName(ctx.model, samples[lower].clipName) : nullptr;
        const AnimationClip* c1 = (upper >= 0 && upper < (int)samples.size()) ? FindClipByName(ctx.model, samples[upper].clipName) : nullptr;

        // どちらも「1周のうち phase の位置」をサンプルする（クリップごとの秒数に直す）
        SampleLocalPose(ctx.model.skeleton, c0, c0 ? phase * c0->duration : 0.0f, out);
        if (c0 && c1 && c1 != c0 && weight > 0.0f) {
            SampleLocalPose(ctx.model.skeleton, c1, phase * c1->duration, scratch);
            BlendPose(out, scratch, weight, out);
        }
    }

} // namespace uel