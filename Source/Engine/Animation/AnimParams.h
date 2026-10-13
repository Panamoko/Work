#pragma once
#include <string>
#include <vector>

namespace uel {

    // アニメーショングラフに渡す値（UE の AnimInstance の変数、Unity の Animator のパラメータに当たる）。
    // ゲーム側は「速度」「攻撃した」などをここに書くだけで、どのクリップを出すかはグラフが決める。
    enum class AnimParamType { Float, Bool, Trigger };   // 保存するので並び順を変えない（増やすときは末尾）

    struct AnimParam {
        std::string   name;
        AnimParamType type = AnimParamType::Float;
        float         value = 0.0f;     // Bool / Trigger は 0 か 1
    };

    class AnimParams {
    public:
        std::vector<AnimParam> items;

        int Find(const std::string& name) const {
            for (int i = 0; i < (int)items.size(); ++i) if (items[i].name == name) return i;
            return -1;
        }

        // 無ければ作る。ゲーム側が先に Set しても、グラフ側が先に読んでも困らないようにするため。
        AnimParam& Ensure(const std::string& name, AnimParamType type) {
            const int i = Find(name);
            if (i >= 0) return items[i];
            AnimParam p; p.name = name; p.type = type;
            items.push_back(p);
            return items.back();
        }

        void SetFloat(const std::string& name, float v) { Ensure(name, AnimParamType::Float).value = v; }
        void SetBool(const std::string& name, bool v) { Ensure(name, AnimParamType::Bool).value = v ? 1.0f : 0.0f; }
        void SetTrigger(const std::string& name) { Ensure(name, AnimParamType::Trigger).value = 1.0f; }

        // その名前のパラメータがあるときだけ値を書く（無ければ作らずに false を返す）。
// ゲーム側のコードが使う。無いときに作ってしまうと、関係ないグラフにパラメータが増えていくため。
        bool SetIfExists(const std::string& name, float v) {
            const int i = Find(name);
            if (i < 0) return false;
            items[i].value = v;
            return true;
        }


        float GetFloat(const std::string& name, float fallback = 0.0f) const {
            const int i = Find(name);
            return (i >= 0) ? items[i].value : fallback;
        }

        bool GetBool(const std::string& name) const { return GetFloat(name, 0.0f) > 0.5f; }

        // Trigger は「1回だけ効く」値。立っていれば下ろして true を返す。
        bool ConsumeTrigger(const std::string& name) {
            const int i = Find(name);
            if (i < 0 || items[i].value < 0.5f) return false;
            items[i].value = 0.0f;
            return true;
        }
    };

} // namespace uel