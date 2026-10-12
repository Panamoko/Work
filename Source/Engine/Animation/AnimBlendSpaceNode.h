#pragma once
#include "Engine/Animation/AnimNode.h"
#include <string>
#include <vector>

namespace uel {

    // 1つの値（速度など）に沿って、複数のクリップを混ぜるノード（UE の Blend Space 1D に当たる）。
    //   例： Speed = 0 に Idle、1.5 に Walk、4 に Run を置くと、Speed = 2.75 なら Walk と Run を半分ずつ混ぜる。
    //
    // 混ぜる2つのクリップは「同じ割合の位置」で再生する（位相をそろえる）。
    //   歩き1周が 1.08 秒、走り1周が 0.67 秒でも、どちらも「1周の 30% の位置」を混ぜるので、足の左右がずれない。
    //   1周の長さは、混ぜ具合に応じて2つの長さの間になる（歩きから走りへ、歩調が連続して速くなる）。
    class AnimBlendSpace1DNode : public AnimNode {
    public:
        struct Sample {
            std::string clipName;
            float       position = 0.0f;    // 軸の上で、このクリップが 100% になる値
        };

        // ---- 設定 ----
        std::vector<Sample> samples;        // 並び順は自由（値の近い2つを毎回探す）
        std::string param;                  // 軸にする Float パラメータの名前
        float       speed = 1.0f;
        float       smoothTime = 0.1f;      // 入力の値をなめらかに追いかける時間（秒）。0 ならそのまま使う

        // ---- 実行時の状態 ----
        float value = 0.0f;                 // なめらかにした後の、いまの軸の値
        float phase = 0.0f;                 // 1周の中の位置（0〜1）
        int   lower = -1, upper = -1;       // いま混ぜている2つ（samples の番号。無ければ -1）
        float weight = 0.0f;                // upper 側の割合（0 = lower だけ、1 = upper だけ）

        const char* TypeName() const override { return "BlendSpace1D"; }
        void Update(AnimContext& ctx) override;
        void Evaluate(AnimContext& ctx, LocalPose& out) override;
        void Reset() override { phase = 0.0f; started = false; }
        std::unique_ptr<AnimNode> Clone() const override { return std::make_unique<AnimBlendSpace1DNode>(*this); }

    private:
        bool      started = false;          // 最初の1回は、なめらかにせず入力の値から始める
        LocalPose scratch;                  // 2つ目のクリップの姿勢を入れる作業用
    };

} // namespace uel