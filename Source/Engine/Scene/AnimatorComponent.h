#pragma once
#include "Engine/Scene/Component.h"
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Animation/AnimParams.h"
#include "Engine/Animation/AnimNode.h"
#include "Engine/Animation/AnimMontage.h"

namespace uel {

    // アニメーションを制御する Component（UE の AnimInstance に当たる）。
    // 同じ Entity の SkinnedMeshComponent のモデルに対して「どのクリップを、どう混ぜて再生するか」を決める。
    // これが付いている間、SkinnedMeshComponent 側の clipIndex / time などは使われない。
    // 姿勢の計算そのものは AnimationSystem が行う（この Component は状態と指示だけを持つ）。
    //
    // 動かし方は2通り：
    //   ・graph が空      ：クリップ1つを再生する。Play() で切り替えるとクロスフェードする。
    //   ・graph が入っている：ノードの木が姿勢を決める。ゲーム側は params に値を書くだけ。
    class AnimatorComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<AnimatorComponent>(*this); }

        // ---- 設定 ----
        int   clipIndex = 0;        // いま再生しているクリップ（graph が空のとき）
        float speed = 1.0f;         // 全体の再生速度（graph にも効く）
        bool  loop = true;
        bool  playing = true;
        float blendTime = 0.2f;     // Play() で秒数を指定しなかったときのクロスフェード時間

        // ---- グラフ ----
        AnimParams  params;         // グラフに渡す値。ゲーム側は params.SetFloat("Speed", v) のように書く
        AnimNodePtr graph;          // 根のノード。入っていれば、こちらが姿勢を決める
        std::string graphPath;      // グラフのアセットのパス（Data/Anim/<名前>.animgraph.json）。空なら未保存
        std::vector<AnimMontage> montages;  // モンタージュの定義（グラフのアセットに一緒に保存する）


        // ---- 通知（実行時だけ。保存しない）----
        // このフレームのアニメの計算で発火した通知の名前。AnimationSystem が毎フレーム入れ直す。
        // ゲーム側（Component の Update）が読むのは、次のフレームになる。
        std::vector<std::string> events;
        std::string lastEvent;              // 最後に発火した通知（ノードエディタの表示用）
        float       lastEventAge = 0.0f;    // それから何秒たったか
        bool WasFired(const char* name) const {
            for (const std::string& e : events) if (e == name) return true;
            return false;
        }

        // ---- 実行時の状態（保存しない）----
        float time = 0.0f;          // clipIndex のクリップの再生位置
        bool  finished = false;     // ループしないクリップが最後まで再生された

        // クロスフェード：fadeElapsed < fadeDuration の間、「切り替え前」から今のクリップへ混ぜる
        float fadeElapsed = 0.0f;
        float fadeDuration = 0.0f;
        bool  fromSnapshot = false; // true：止めた姿勢（fromPose）から混ぜる / false：fromClip を再生し続けながら混ぜる
        int   fromClip = -1;
        float fromTime = 0.0f;
        bool  fromLoop = true;
        LocalPose fromPose;         // 切り替えた瞬間の姿勢
        LocalPose lastPose;         // 直前のフレームに出した姿勢（AnimationSystem が書く）

        bool IsFading() const { return fadeDuration > 0.0f && fadeElapsed < fadeDuration; }
        // ---- モンタージュ（実行時だけ。保存しない）----
// グラフ（グラフが無ければクリップ1つ）の出した姿勢の上に、クリップを1回だけ重ねて再生する（UE の Montage に当たる）。
// 下のグラフは止めずに進めておくので、モンタージュが抜けると「今の」歩きや待機に戻る。
// 重みの変わり方：始めに blendIn 秒で 0→1、クリップの終わりの blendOut 秒で 1→0。StopMontage でも 1→0。
        struct MontageState {
            std::string  name;              // 再生中のモンタージュの名前（PlayMontageClip で始めたときは空）
            AnimClipNode clip;             // 再生するクリップ（ループしない）。通知もここから出る
            bool  active = false;           // 再生中（抜けている最中も含む）
            bool  stopping = false;         // StopMontage が呼ばれ、抜けている最中
            float blendIn = 0.1f;           // 入るのにかける秒数
            float blendOut = 0.2f;          // クリップの終わりで抜けるのにかける秒数
            float stopTime = 0.2f;          // StopMontage で抜けるのにかける秒数
            float elapsed = 0.0f;           // 始めてからの秒数
            float stopElapsed = 0.0f;       // StopMontage から何秒たったか
            float weight = 0.0f;            // いまの重み 0～1（AnimationSystem が書く。表示用）
            bool  fromSnapshot = false;     // 再生中のモンタージュに割り込んだ：止めた姿勢（fromPose）から混ぜる
            LocalPose fromPose;
        };
        MontageState montage;
        LocalPose finalPose;        // 直前のフレームに最終的に出した姿勢（モンタージュ込み。AnimationSystem が書く）

        // クリップの名前を指定して、モンタージュを頭から再生する。
        // 再生中に呼ぶと、いま画面に出ている姿勢から、新しいモンタージュへ混ぜて切り替える。
        // 名前のクリップがモデルに無ければ、何も再生しない（次の姿勢の計算で取り消される）。
        void PlayMontageClip(const std::string& clipName, float blendIn = 0.1f, float blendOut = 0.2f, float rate = 1.0f)
        {
            MontageState& m = montage;
            m.fromSnapshot = m.active && m.weight > 0.0f && !finalPose.empty();
            if (m.fromSnapshot) m.fromPose = finalPose;
            m.clip.clipName = clipName;
            m.clip.loop = false;
            m.clip.speed = rate;
            m.clip.speedParam.clear();
            m.clip.notifies.clear();
            m.clip.Reset();
            m.name.clear();
            m.active = true;
            m.stopping = false;
            m.blendIn = blendIn;
            m.blendOut = blendOut;
            m.elapsed = 0.0f;
            m.stopElapsed = 0.0f;
        }

        // 再生中のモンタージュを、blendOut 秒かけて抜ける（下のグラフの姿勢へ戻る）
        void StopMontage(float blendOut = 0.2f)
        {
            if (!montage.active || montage.stopping) return;
            montage.stopping = true;
            montage.stopTime = blendOut;
            montage.stopElapsed = 0.0f;
        }

        bool IsMontagePlaying() const { return montage.active; }
        bool IsMontagePlaying(const std::string& name) const { return montage.active && montage.name == name; }

        // モンタージュの定義を名前で探す（無ければ nullptr）
        const AnimMontage* FindMontage(const std::string& name) const {
            for (const AnimMontage& d : montages) if (d.name == name) return &d;
            return nullptr;
        }

        // 定義どおりにモンタージュを再生する（クリップ・入り方・抜け方・速度・通知）
        void PlayMontage(const AnimMontage& def)
        {
            PlayMontageClip(def.clipName, def.blendIn, def.blendOut, def.rate);
            montage.clip.notifies = def.notifies;
            montage.name = def.name;
        }

        // 名前で指定してモンタージュを再生する（ゲーム側はこれを使う）。その名前の定義が無ければ何もせず false
        bool PlayMontage(const std::string& name)
        {
            const AnimMontage* def = FindMontage(name);
            if (!def) return false;
            PlayMontage(*def);
            return true;
        }

        void CancelFade() { fadeElapsed = 0.0f; fadeDuration = 0.0f; }

        // クリップを切り替える。blend 秒かけて前の動きから混ぜる（マイナスなら blendTime を使う）。
        // 同じクリップを指定したときは何もしない（頭から再生し直したいときは restart = true）。
        void Play(int clip, float blend = -1.0f, bool restart = false)
        {
            if (clip == clipIndex && !restart) return;
            const float d = (blend < 0.0f) ? blendTime : blend;
            if (IsFading() || clip == clipIndex) {
                // フェードの途中で切り替えた／同じクリップの再生し直し：
                // いま画面に出ている姿勢を止めて、そこから混ぜる（2つを同時に追いかけない）
                fromSnapshot = true;
                fromPose = lastPose;
            }
            else {
                fromSnapshot = false;
                fromClip = clipIndex; fromTime = time; fromLoop = loop;
            }
            clipIndex = clip;
            time = 0.0f;
            finished = false;
            fadeElapsed = 0.0f;
            fadeDuration = d;
        }
    };

} // namespace uel