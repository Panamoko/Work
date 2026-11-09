#pragma once
#include <filesystem>

namespace uel {
    // シェーダのホットリロード（開発用）。
    //   Shader/ の .hlsl / .hlsli のどれかが保存されたら、全部の .hlsl を D3DCompileFromFile でコンパイルし直し、
    //   できたバイトコードを ShaderLoader に渡す（以後 .cso の代わりに使われる）。.cso のファイルは書き換えない。
    //   シェーダの種類は名前の終わりで決める（〜VS = 頂点、〜PS = ピクセル）。関数名はどれも main。
    //   コンパイルできなかったシェーダは、前のものを使い続ける（エラーは VS の「出力」に [Shader] で出る）。
    //   Shader/ フォルダが無いとき（exe だけを配ったときなど）は何もしない。
    class ShaderHotReload {
    public:
        void Initialize(const char* sourceDir);

        // 毎フレーム呼ぶ。コンパイルし直したシェーダが1つ以上あれば true を返す（呼んだ側でシェーダを作り直す）
        bool Update(float dt);

    private:
        std::filesystem::file_time_type NewestWriteTime() const;   // .hlsl / .hlsli のいちばん新しい保存時刻
        int CompileAll();                                           // コンパイルできた数を返す

        std::filesystem::path           dir;
        bool                            enabled = false;
        std::filesystem::file_time_type lastSeen{};
        float                           pollTimer = 0.0f;     // 次にファイルの時刻を調べるまでの時間
        float                           settleTimer = -1.0f;  // 0 以上なら「変更あり。この時間が過ぎたらコンパイル」
    };
}