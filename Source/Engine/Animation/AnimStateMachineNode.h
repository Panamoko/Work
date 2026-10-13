#pragma once
#include "Engine/Animation/AnimNode.h"
#include <string>
#include <vector>

namespace uel {

    // ステートマシンのノード（UE の AnimGraph の State Machine に当たる）。
    //   ・状態（State）：それぞれが子ノードを1つ持つ（クリップ、Blend Space など）。再生されるのは「今の状態」だけ。
    //   ・遷移（Transition）：条件がそろったら、ブレンド時間をかけて別の状態へ移る。
    // ゲーム側は状態を直接切り替えない。パラメータを書くだけで、どの状態にいるかはこのノードが決める。
    class AnimStateMachineNode : public AnimNode {
    public:
        enum class CondOp { Greater, Less, IsTrue, IsFalse };   // 保存するので並び順を変えない（増やすときは末尾）

        // 条件1つ。パラメータの種類で意味が決まる：
        //   Float   … Greater / Less で threshold と比べる
        //   Bool    … IsTrue / IsFalse
        //   Trigger … 立っていれば成立（op は見ない）
        struct Condition {
            std::string param;
            CondOp      op = CondOp::Greater;
            float       threshold = 0.0f;
        };

        struct State {
            std::string name;
            AnimNodePtr node;               // この状態で再生するもの（空なら bind ポーズ）
            float editorX = 0.0f, editorY = 0.0f;   // ノードエディタ上の箱の位置
        };

        struct Transition {
            int   from = -1;                // 移る前の状態（-1 = どの状態からでも）
            int   to = 0;                   // 移る先の状態
            float blendTime = 0.2f;         // 何秒かけて混ぜるか
            bool  waitForEnd = false;       // true：移る前の状態のクリップが最後まで再生されてから移る（攻撃→待機など）
            std::vector<Condition> conditions;  // すべて成り立ったら移る。空で waitForEnd も false の遷移は無効
        };

        // ---- 設定 ----
        std::vector<State>      states;
        std::vector<Transition> transitions;    // 上から順に調べて、最初に成り立ったものを使う
        int entry = 0;                           // 最初の状態
        float anyX = -190.0f, anyY = 8.0f;      // ノードエディタ上の「Any」の箱の位置

        // ---- 実行時の状態 ----
        int   current = -1;                 // 今の状態（-1 = まだ始まっていない）
        int   fromState = -1;               // 移る前の状態（ブレンド中だけ意味がある）
        bool  fromSnapshot = false;         // true：止めた姿勢（fromPose）から混ぜる / false：fromState を再生し続けながら混ぜる
        float fadeElapsed = 0.0f;
        float fadeDuration = 0.0f;

        bool IsFading() const { return fadeDuration > 0.0f && fadeElapsed < fadeDuration; }

        const char* TypeName() const override { return "StateMachine"; }
        void Update(AnimContext& ctx) override;
        void Evaluate(AnimContext& ctx, LocalPose& out) override;
        void Reset() override { current = -1; fromState = -1; fadeElapsed = fadeDuration = 0.0f; }
        bool HasNotify(const std::string& name) const override;     // どれかの状態の中身が、この名前の通知を持っているか
        std::unique_ptr<AnimNode> Clone() const override { return std::make_unique<AnimStateMachineNode>(*this); }

        // 状態を1つ消す（遷移の番号を詰め直す。エディタ用）
        void RemoveState(int index);

    private:
        bool ValidState(int i) const { return i >= 0 && i < (int)states.size(); }
        void StartTransition(const Transition& tr);

        LocalPose fromPose;                 // 遷移を始めた瞬間の姿勢（ブレンドの途中で次の遷移が起きたとき用）
        LocalPose lastPose;                 // 直前のフレームに出した姿勢
        LocalPose scratch;
    };

} // namespace uel