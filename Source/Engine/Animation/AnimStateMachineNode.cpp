#include "Engine/Animation/AnimStateMachineNode.h"
#include <cmath>

namespace uel {
    namespace {

        bool TestCondition(const AnimStateMachineNode::Condition& c, const AnimParams& params)
        {
            const int i = params.Find(c.param);
            if (i < 0) return false;                        // 無いパラメータの条件は成り立たない
            const AnimParam& p = params.items[i];
            switch (p.type) {
            case AnimParamType::Float:   return (c.op == AnimStateMachineNode::CondOp::Less) ? (p.value < c.threshold) : (p.value > c.threshold);
            case AnimParamType::Bool:    return (c.op == AnimStateMachineNode::CondOp::IsFalse) ? (p.value < 0.5f) : (p.value > 0.5f);
            case AnimParamType::Trigger: return p.value > 0.5f;
            }
            return false;
        }

    } // namespace

    void AnimStateMachineNode::StartTransition(const Transition& tr)
    {
        if (IsFading()) {
            // ブレンドの途中で次の遷移が起きた：いま出ている姿勢を止めて、そこから混ぜる
            fromSnapshot = true;
            fromPose = lastPose;
        }
        else {
            fromSnapshot = false;
            fromState = current;
        }
        current = tr.to;
        if (AnimNode* n = states[current].node.get()) n->Reset();      // 移った先は頭から再生する
        fadeElapsed = 0.0f;
        fadeDuration = tr.blendTime;
    }

    void AnimStateMachineNode::Update(AnimContext& ctx)
    {
        if (states.empty()) { current = -1; return; }

        // まだ始まっていない（または状態が消されて番号が外れた）：最初の状態から始める
        if (!ValidState(current)) {
            current = ValidState(entry) ? entry : 0;
            fromState = -1; fadeElapsed = fadeDuration = 0.0f;
            if (AnimNode* n = states[current].node.get()) n->Reset();
        }

        // 1) 遷移を調べる（1フレームに1つまで）
        for (const Transition& tr : transitions) {
            if (!ValidState(tr.to) || tr.to == current) continue;
            if (tr.from != -1 && tr.from != current) continue;
            if (tr.conditions.empty() && !tr.waitForEnd) continue;         // 条件が何も無い遷移は使わない
            if (tr.waitForEnd) {
                AnimNode* cn = states[current].node.get();
                if (!cn || !cn->IsFinished()) continue;
            }
            bool ok = true;
            for (const Condition& c : tr.conditions) if (!TestCondition(c, ctx.params)) { ok = false; break; }
            if (!ok) continue;
            StartTransition(tr);
            break;
        }

        // 2) 今の状態を進める。ブレンド中は、移る前の状態も止めずに進める
        if (AnimNode* n = states[current].node.get()) n->Update(ctx);
        if (IsFading()) {
            fadeElapsed += std::fabs(ctx.dt);
            if (!fromSnapshot && ValidState(fromState) && fromState != current)
                if (AnimNode* n = states[fromState].node.get()) n->Update(ctx);
        }
    }

    void AnimStateMachineNode::Evaluate(AnimContext& ctx, LocalPose& out)
    {
        if (!ValidState(current)) { out.clear(); return; }

        // 今の状態の姿勢
        if (AnimNode* n = states[current].node.get()) n->Evaluate(ctx, out);
        else SampleLocalPose(ctx.model.skeleton, nullptr, 0.0f, out);

        // ブレンド中なら、移る前の姿勢から混ぜる
        if (IsFading()) {
            float a = fadeElapsed / fadeDuration;
            if (a > 1.0f) a = 1.0f;
            a = a * a * (3.0f - 2.0f * a);      // smoothstep

            const LocalPose* from = nullptr;
            if (fromSnapshot) {
                if (fromPose.size() == out.size()) from = &fromPose;
            }
            else if (ValidState(fromState) && fromState != current) {
                if (AnimNode* n = states[fromState].node.get()) {
                    n->Evaluate(ctx, scratch);
                    if (scratch.size() == out.size()) from = &scratch;
                }
            }
            if (from) BlendPose(*from, out, a, out);
        }

        lastPose = out;
    }

    void AnimStateMachineNode::RemoveState(int index)
    {
        if (!ValidState(index)) return;
        states.erase(states.begin() + index);

        // この状態を使っていた遷移は消し、それより後ろの番号は1つ詰める
        std::vector<Transition> kept;
        for (Transition tr : transitions) {
            if (tr.from == index || tr.to == index) continue;
            if (tr.from > index) --tr.from;
            if (tr.to > index) --tr.to;
            kept.push_back(std::move(tr));
        }
        transitions = std::move(kept);

        if (entry == index) entry = 0;
        else if (entry > index) --entry;
        Reset();    // 番号が変わったので、最初の状態からやり直す
    }

} // namespace uel