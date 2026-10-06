#include "Engine/Systems/AnimationSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Scene/AnimatorComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Graphics/SkinnedRenderModel.h"
#include "Engine/Animation/AnimationPose.h"
#include <cmath>
#include <memory>

namespace uel {
    namespace {

        const AnimationClip* ClipAt(const SkinnedModel& cpu, int index) {
            return (index >= 0 && index < (int)cpu.clips.size()) ? &cpu.clips[index] : nullptr;
        }

        // 再生位置を dt ぶん進める。ループしないクリップが端まで来たら true。
        bool AdvanceTime(float& time, const AnimationClip* clip, float dt, bool loop) {
            if (!clip) return false;
            const float dur = clip->duration;
            time += dt;
            if (dur <= 0.0f) return false;
            if (loop) { time = std::fmod(time, dur); if (time < 0.0f) time += dur; return false; }
            if (time >= dur) { time = dur; return true; }
            if (time < 0.0f) { time = 0.0f; return true; }      // 逆再生で先頭まで戻った
            return false;
        }

        // AnimatorComponent が無いとき：SkinnedMeshComponent のクリップ1つをそのまま再生する
        void EvaluateSingleClip(SkinnedMeshComponent& sk, const SkinnedModel& cpu, float dt, LocalPose& pose)
        {
            const AnimationClip* clip = ClipAt(cpu, sk.clipIndex);
            if (sk.playing) AdvanceTime(sk.time, clip, dt * sk.speed, sk.loop);
            SampleLocalPose(cpu.skeleton, clip, sk.time, pose);
        }

        // AnimatorComponent があるとき：今のクリップを再生し、切り替え中なら前の動きから混ぜる
        void EvaluateAnimator(AnimatorComponent& an, const SkinnedModel& cpu, float dt, LocalPose& pose, LocalPose& scratch)
        {
            const size_t boneCount = cpu.skeleton.bones.size();
            const float step = an.playing ? dt * an.speed : 0.0f;

            // 1) 今のクリップ
            const AnimationClip* cur = ClipAt(cpu, an.clipIndex);
            if (an.playing && AdvanceTime(an.time, cur, step, an.loop)) an.finished = true;
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
                    if (an.playing) AdvanceTime(an.fromTime, fc, step, an.fromLoop);  // 前のクリップも止めずに進める
                    SampleLocalPose(cpu.skeleton, fc, an.fromTime, scratch);
                    from = &scratch;
                }
                if (from) BlendPose(*from, pose, a, pose);
            }

            an.lastPose = pose;     // 次に Play() が呼ばれたとき、ここから混ぜられるように覚えておく
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

            // 1) ローカル姿勢を決める（AnimatorComponent があればそちらが決める）
            if (AnimatorComponent* an = slot->GetComponent<AnimatorComponent>())
                EvaluateAnimator(*an, cpu, dt, pose, scratch);
            else
                EvaluateSingleClip(*sk, cpu, dt, pose);

            // 2) 行列にして SkinnedMeshComponent に入れる
            ComputeGlobalPose(cpu.skeleton, pose, sk->boneGlobal);
            BuildPalette(cpu.skeleton, sk->boneGlobal, sk->palette);
        }
    }

} // namespace uel