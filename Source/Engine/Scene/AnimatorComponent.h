#pragma once
#include "Engine/Scene/Component.h"
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Animation/AnimParams.h"
#include "Engine/Animation/AnimNode.h"

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