#include "Engine/Systems/AnimationSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Graphics/SkinnedRenderModel.h"
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Animation/AnimNode.h"
#include <memory>

namespace uel {
    namespace {

        const AnimationClip* ClipAt(const SkinnedModel& cpu, int index) {
            return (index >= 0 && index < (int)cpu.clips.size()) ? &cpu.clips[index] : nullptr;
        }

        // AnimatorComponent が無いとき：SkinnedMeshComponent のクリップ1つをそのまま再生する
        void EvaluateSingleClip(SkinnedMeshComponent& sk, const SkinnedModel& cpu, float dt, LocalPose& pose)
        {
            const AnimationClip* clip = ClipAt(cpu, sk.clipIndex);
            if (sk.playing) AdvanceClipTime(sk.time, clip, dt * sk.speed, sk.loop);
            SampleLocalPose(cpu.skeleton, clip, sk.time, pose);
        }

        // AnimatorComponent にグラフがあるとき：ノードの木に姿勢を決めさせる
        void EvaluateGraph(AnimatorComponent& an, const SkinnedModel& cpu, float dt, LocalPose& pose)
        {
            AnimContext ctx{ cpu, an.params, an.playing ? dt * an.speed : 0.0f };
            ctx.events = &an.events;        // 発火した通知の名前を、ここへ入れてもらう
            an.graph->Update(ctx);          // 1) 時間を進める
            an.graph->Evaluate(ctx, pose);  // 2) 姿勢を出す
            for (auto& p : an.params.items) if (p.type == AnimParamType::Trigger) p.value = 0.0f;   // Trigger は1フレームだけ有効
            if (pose.size() != cpu.skeleton.bones.size())
                SampleLocalPose(cpu.skeleton, nullptr, 0.0f, pose);     // ノードが何も出さなかったら bind ポーズ
            an.lastPose = pose;
        }

        // AnimatorComponent にグラフが無いとき：今のクリップを再生し、切り替え中なら前の動きから混ぜる
        void EvaluateAnimator(AnimatorComponent& an, const SkinnedModel& cpu, float dt, LocalPose& pose, LocalPose& scratch)
        {
            const size_t boneCount = cpu.skeleton.bones.size();
            const float step = an.playing ? dt * an.speed : 0.0f;

            // 1) 今のクリップ
            const AnimationClip* cur = ClipAt(cpu, an.clipIndex);
            if (an.playing && AdvanceClipTime(an.time, cur, step, an.loop)) an.finished = true;
            SampleLocalPose(cpu.skeleton, cur, an.time, pose);

            // 2) クロスフェード中なら「切り替え前」を求めて、そこから今のクリップへ混ぜる
            if (an.IsFading()) {
                if (an.playing) an.fadeElapsed += dt;
                float a = an.fadeElapsed / an.fadeDuration;
                if (a > 1.0f) a = 1.0f;
                a = a * a * (3.0f - 2.0f * a);      // 出だしと終わりをなめらかにする（smoothstep）

                const LocalPose* from = nullptr;
                if (an.fromSnapshot) {
                    if (an.fromPose.size() == boneCount) from = &an.fromPose;      // モデルが変わっていたら使わない
                }
                else {
                    const AnimationClip* fc = ClipAt(cpu, an.fromClip);
                    if (an.playing) AdvanceClipTime(an.fromTime, fc, step, an.fromLoop);  // 前のクリップも止めずに進める
                    SampleLocalPose(cpu.skeleton, fc, an.fromTime, scratch);
                    from = &scratch;
                }
                if (from) BlendPose(*from, pose, a, pose);
            }

            an.lastPose = pose;     // 次に Play() が呼ばれたとき、ここから混ぜられるように覚えておく
        }

        float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v; }
        float SmoothStep01(float v) { v = Clamp01(v); return v * v * (3.0f - 2.0f * v); }

        // モンタージュ：グラフ（またはクリップ1つ）の出した姿勢 pose の上に、モンタージュのクリップを重ねる
        void ApplyMontage(AnimatorComponent& an, const SkinnedModel& cpu, float dt, LocalPose& pose, LocalPose& scratch)
        {
            AnimatorComponent::MontageState& m = an.montage;
            if (!m.active) { m.weight = 0.0f; return; }

            const AnimationClip* clip = m.clip.FindClip(cpu);
            if (!clip || clip->duration <= 0.0f) { m.active = false; m.weight = 0.0f; return; }   // このモデルに無いクリップ：取り消す

            // 1) 時間を進める（通知も、グラフの通知と同じ events に入る）
            AnimContext ctx{ cpu, an.params, an.playing ? dt * an.speed : 0.0f };
            ctx.events = &an.events;
            m.clip.Update(ctx);
            if (an.playing) {
                m.elapsed += dt;
                if (m.stopping) m.stopElapsed += dt;
            }

            // 2) 重み。入る・終わりで抜ける・Stop で抜ける、の3つのうち一番小さいもの
            const float wIn = (m.fromSnapshot || m.blendIn <= 0.0f) ? 1.0f : m.elapsed / m.blendIn;
            const float rate = m.clip.speed * an.speed;
            const float left = (rate >= 0.0f) ? clip->duration - m.clip.time : m.clip.time;      // 端までの残り（クリップの秒数。逆再生なら頭まで）
            const float remain = (std::fabs(rate) > 1e-4f) ? left / std::fabs(rate) : 1e9f;       // それを実際の秒数にしたもの
            const float wEnd = (m.blendOut > 0.0f) ? remain / m.blendOut : (m.clip.finished ? 0.0f : 1.0f);
            const float wStop = !m.stopping ? 1.0f : (m.stopTime > 0.0f) ? 1.0f - m.stopElapsed / m.stopTime : 0.0f;
            float w = Clamp01(wIn);
            if (wEnd < w) w = Clamp01(wEnd);
            if (wStop < w) w = Clamp01(wStop);
            if ((m.clip.finished && wEnd <= 0.0f) || (m.stopping && wStop <= 0.0f)) {   // 抜け終わった
                m.active = false; m.weight = 0.0f; m.fromSnapshot = false;
                return;
            }
            m.weight = w;

            // 3) 下の姿勢とモンタージュの姿勢を混ぜる
            //    maskBone があれば、その骨から先（上半身など）だけに重ねる。下半身は下のグラフの動き（移動）のまま
            m.clip.Evaluate(ctx, scratch);
            if (m.maskBone.empty()) BlendPose(pose, scratch, SmoothStep01(w), pose);
            else {
                if (m.maskDirty || m.mask.size() != pose.size()) {      // 再生し直した、またはモデルが変わった
                    BuildBoneMask(cpu.skeleton, m.maskBone, m.maskBlendDepth, m.mask);    // 骨が見つからなければ全部 0（重ならない）
                    m.maskDirty = false;
                }
                BlendPoseMasked(pose, scratch, SmoothStep01(w), m.mask, pose);
            }

            // 再生中のモンタージュに割り込んだ直後：止めた姿勢から、いまの結果へ blendIn 秒かけて移る
            if (m.fromSnapshot) {
                if (m.elapsed < m.blendIn && m.fromPose.size() == pose.size())
                    BlendPose(m.fromPose, pose, SmoothStep01(m.elapsed / m.blendIn), pose);
                else m.fromSnapshot = false;
            }
        }
    } // namespace

    void AnimationSystem::Update(World& world, float dt, SkinnedModelCache& cache, ID3D11Device* device)
    {
        LocalPose pose, scratch;    // Entity ごとに作り直さず、使い回す

        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            SkinnedMeshComponent* sk = slot->GetComponent<SkinnedMeshComponent>();
            if (!sk) continue;

            std::shared_ptr<SkinnedRenderModel> model;
            if (!sk->assetPath.empty()) model = cache.GetOrLoad(device, sk->assetPath, sk->animPath);
            if (!model || !model->Valid()) {
                // モデルが無い・読めない：前のモデルの姿勢が残らないように消しておく
                sk->boneGlobal.clear();
                sk->palette.clear();
                continue;
            }
            const SkinnedModel& cpu = model->Cpu();

            // 1) ローカル姿勢を決める
            //    AnimatorComponent（グラフあり）→ AnimatorComponent（クリップ1つ）→ SkinnedMeshComponent の順に探す
            AnimatorComponent* an = slot->GetComponent<AnimatorComponent>();
            if (an) an->events.clear();     // 前のフレームの通知を捨てる
            if (an && an->graph) EvaluateGraph(*an, cpu, dt, pose);
            else if (an)         EvaluateAnimator(*an, cpu, dt, pose, scratch);
            else                 EvaluateSingleClip(*sk, cpu, dt, pose);
            if (an) {
                ApplyMontage(*an, cpu, dt, pose, scratch);  // モンタージュを上に重ねる（再生していなければ何もしない）
                ApplyAimOffset(cpu.skeleton, an->aim, an->params, pose);   // 視点の上下に合わせて背骨を曲げる（有効なときだけ）
                an->finalPose = pose;                      // 次にモンタージュへ割り込んだとき、ここから混ぜる
            }
            // 一人称のとき：指定した骨（頭など）を潰して、カメラの前に自分の頭が映らないようにする。
// スケールをほぼ 0 にすると、その骨と子の骨（髪や顔）に付いた頂点が1点に集まり、描かれなくなる
            if (sk->firstPerson && !sk->fpHideBone.empty()) {
                const int hb = cpu.skeleton.Find(sk->fpHideBone);
                if (hb >= 0 && hb < (int)pose.size()) pose[hb].s = { 0.0001f, 0.0001f, 0.0001f };
            }

            // ノードエディタの表示用：最後に発火した通知と、それからの時間
            if (an) {
                if (!an->events.empty()) { an->lastEvent = an->events.back(); an->lastEventAge = 0.0f; }
                else an->lastEventAge += dt;
            }

            // 2) 行列にして SkinnedMeshComponent に入れる
            ComputeGlobalPose(cpu.skeleton, pose, sk->boneGlobal);
            BuildPalette(cpu.skeleton, sk->boneGlobal, sk->palette);
        }
    }

} // namespace uel