#pragma once
#include "Engine/Animation/AnimationPose.h"
#include "Engine/Animation/AnimParams.h"
#include "Engine/Animation/SkinnedModel.h"
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace uel {

    // 再生位置を dt ぶん進める。ループしないクリップが端まで来たら true。
    inline bool AdvanceClipTime(float& time, const AnimationClip* clip, float dt, bool loop) {
        if (!clip) return false;
        const float dur = clip->duration;
        time += dt;
        if (dur <= 0.0f) return false;
        if (loop) { time = std::fmod(time, dur); if (time < 0.0f) time += dur; return false; }
        if (time >= dur) { time = dur; return true; }
        if (time < 0.0f) { time = 0.0f; return true; }      // 逆再生で先頭まで戻った
        return false;
    }

    // ノードが姿勢を出すときに受け取るもの
    struct AnimContext {
        const SkinnedModel& model;      // 骨格とクリップ
        AnimParams& params;     // グラフに渡された値（Trigger を下ろすので const ではない）
        float               dt;         // 進める時間（秒）
        std::vector<std::string>* events = nullptr;     // このフレームに発火した通知の名前を入れる先（無ければ nullptr）
    };

    // ポーズを出すノードの基底（UE の AnimGraph のノードに当たる）。
    // ノードは木になっていて、根のノードが出した姿勢が最終結果になる。
    // 1フレームに「Update（時間を進める）→ Evaluate（姿勢を出す）」の順で1回ずつ呼ばれる。
    class AnimNode {
    public:
        virtual ~AnimNode() = default;
        virtual const char* TypeName() const = 0;
        virtual void Update(AnimContext& ctx) = 0;
        virtual void Evaluate(AnimContext& ctx, LocalPose& out) = 0;
        virtual void Reset() {}                                 // 頭から再生し直す
        virtual bool IsFinished() const { return false; }       // ループしないクリップが最後まで再生されたら true
        virtual bool HasNotify(const std::string&) const { return false; }     // この名前の通知を、自分か子が持っているか
        virtual std::unique_ptr<AnimNode> Clone() const = 0;    // 子も含めて複製する
    };

    // ノードの持ち主。コピーすると中身を Clone() する。
    // これを使うと、ノードを持つ Component の Clone() が「普通のコピー」のままで済む。
    struct AnimNodePtr {
        std::unique_ptr<AnimNode> node;

        AnimNodePtr() = default;
        AnimNodePtr(std::unique_ptr<AnimNode> n) : node(std::move(n)) {}
        AnimNodePtr(const AnimNodePtr& o) : node(o.node ? o.node->Clone() : nullptr) {}
        AnimNodePtr(AnimNodePtr&&) noexcept = default;
        AnimNodePtr& operator=(const AnimNodePtr& o) { if (this != &o) node = o.node ? o.node->Clone() : nullptr; return *this; }
        AnimNodePtr& operator=(AnimNodePtr&&) noexcept = default;

        AnimNode* get() const { return node.get(); }
        AnimNode* operator->() const { return node.get(); }
        explicit operator bool() const { return node != nullptr; }
        void reset() { node.reset(); }
    };

    // クリップを1つ再生するノード。木の「葉」になる。
    class AnimClipNode : public AnimNode {
    public:
        // 通知（UE の Anim Notify に当たる）。再生位置がこの時刻を通り過ぎたら、名前をゲーム側へ知らせる。
        //   例：パンチのクリップの 0.42 秒に "Hit" を置くと、腕が当たる瞬間にダメージを出せる
        struct Notify {
            float       time = 0.0f;    // クリップの頭からの秒数
            std::string name;
        };

        // ---- 設定 ----
        std::string clipName;           // 名前で指定する（番号だと、モデルを差し替えたときにずれるため）
        bool        loop = true;
        float       speed = 1.0f;
        std::string speedParam;         // 空でなければ、この Float パラメータを再生速度に掛ける
        std::vector<Notify> notifies;

        // ---- 実行時の状態 ----
        float time = 0.0f;
        bool  finished = false;         // ループしないクリップが最後まで再生された

        const char* TypeName() const override { return "Clip"; }
        void Update(AnimContext& ctx) override;
        void Evaluate(AnimContext& ctx, LocalPose& out) override;
        void Reset() override { time = 0.0f; finished = false; fresh = true; }
        bool IsFinished() const override { return finished; }
        bool HasNotify(const std::string& name) const override {
            for (const Notify& n : notifies) if (n.name == name) return true;
            return false;
        }
        std::unique_ptr<AnimNode> Clone() const override { return std::make_unique<AnimClipNode>(*this); }

        // このモデルでの clipName のクリップ（無ければ nullptr）
        const AnimationClip* FindClip(const SkinnedModel& model);

    private:
        bool fresh = true;              // 頭から再生し始めたばかり（時刻 0 に置いた通知も、最初の1回で発火させるための印）

        // 名前からクリップを探した結果を覚えておく（毎フレーム文字列で探さないため）
        const SkinnedModel* cachedModel = nullptr;
        std::string         cachedName;
        int                 cachedIndex = -1;
    };

} // namespace uel