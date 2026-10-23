#pragma once
#include "Engine/Animation/AnimNode.h"
#include <string>

namespace uel {

    // 2つの姿勢を、骨ごとに混ぜるノード（UE の Layered blend per bone に当たる）。
    //   basePose  … 下地。全身の動き（移動のステートマシンなど）
    //   blendPose … 重ねる側。maskBone から先の骨（上半身など）だけに効く（構え・攻撃など）
    // 例：maskBone = "spine" にすると、腰から下は basePose（走り）、背骨から上は blendPose（パンチ）になる。
    //
    // 2つの入力は、重みが 0 のときも両方とも時間を進める（重みを上げた瞬間に、止まっていた動きが出ないように）。
    class AnimLayeredBlendNode : public AnimNode {
    public:
        // ---- 設定 ----
        AnimNodePtr basePose;               // 下地（無ければ bind ポーズ）
        AnimNodePtr blendPose;              // 重ねる側（無ければ下地のまま）
        std::string maskBone;               // この骨から先に重ねる。空なら全身に重ねる
        int         maskBlendDepth = 0;     // 境目を何段の骨でなじませるか（0 = 境目の骨から全部 100%）
        float       weight = 1.0f;          // 重ねる割合（0～1）
        std::string weightParam;            // 空でなければ、この Float パラメータ（0～1）を weight に掛ける

        // ---- 実行時の状態 ----
        float lastWeight = 0.0f;            // 前回の Evaluate で使った重み（表示用）

        const char* TypeName() const override { return "LayeredBlend"; }
        void Update(AnimContext& ctx) override;
        void Evaluate(AnimContext& ctx, LocalPose& out) override;
        void Reset() override;
        bool IsFinished() const override { return basePose && basePose->IsFinished(); }    // 下地の動きで判断する
        bool HasNotify(const std::string& name) const override {
            return (basePose && basePose->HasNotify(name)) || (blendPose && blendPose->HasNotify(name));
        }
        std::unique_ptr<AnimNode> Clone() const override { return std::make_unique<AnimLayeredBlendNode>(*this); }

    private:
        // maskBone から作った骨ごとの重み。骨の名前・段数・骨格が変わったときだけ作り直す
        BoneMask        mask;
        const Skeleton* maskSkeleton = nullptr;
        std::string     maskBuiltBone;
        int             maskBuiltDepth = -1;
        LocalPose       scratch;            // blendPose の姿勢を入れる作業用

        void UpdateMask(const Skeleton& skel);
    };

} // namespace uel
