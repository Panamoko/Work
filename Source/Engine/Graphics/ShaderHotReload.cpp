#include "Engine/Graphics/ShaderHotReload.h"
#include "Engine/Graphics/ShaderLoader.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <chrono>
#include <string>
#include <system_error>
#include <vector>
#pragma comment(lib, "d3dcompiler.lib")

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        constexpr float kPollInterval = 0.5f;   // ファイルの保存時刻を調べる間隔（秒）
        constexpr float kSettleTime = 0.3f;     // 変更を見つけてからコンパイルするまで待つ時間（保存の途中を読まないため）

        bool IsShaderSource(const fs::path& p)
        {
            const fs::path ext = p.extension();
            return ext == ".hlsl" || ext == ".hlsli";
        }

        // 名前の終わり（VS / PS）から、コンパイルの種類を決める。どちらでもなければ nullptr
        const char* TargetFromName(const std::string& name)
        {
            if (name.size() >= 2) {
                const std::string tail = name.substr(name.size() - 2);
                if (tail == "VS") return "vs_5_0";
                if (tail == "PS") return "ps_5_0";
            }
            return nullptr;
        }

        std::string BlobText(ID3DBlob* blob)
        {
            return blob ? std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize()) : std::string();
        }

        void Report(const std::string& message)
        {
            OutputDebugStringA(("[Shader] " + message + "\n").c_str());
        }
    }

    void ShaderHotReload::Initialize(const char* sourceDir)
    {
        std::error_code ec;
        dir = sourceDir;
        enabled = fs::is_directory(dir, ec);
        if (!enabled) {
            Report(std::string("hot reload is off (folder not found: ") + sourceDir + ")");
            return;
        }
        lastSeen = NewestWriteTime();   // 起動時の状態を覚える（起動直後にはコンパイルしない）
    }

    fs::file_time_type ShaderHotReload::NewestWriteTime() const
    {
        fs::file_time_type newest{};
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || !IsShaderSource(it->path())) continue;
            const fs::file_time_type t = fs::last_write_time(it->path(), ec);
            if (!ec && t > newest) newest = t;
        }
        return newest;
    }

    bool ShaderHotReload::Update(float dt)
    {
        if (!enabled) return false;

        // 0.5 秒ごとに保存時刻を調べる（毎フレーム調べるとファイルの読み取りが多いため）
        pollTimer -= dt;
        if (pollTimer <= 0.0f) {
            pollTimer = kPollInterval;
            const fs::file_time_type newest = NewestWriteTime();
            if (newest != lastSeen) {
                lastSeen = newest;
                settleTimer = kSettleTime;
            }
        }

        // 変更を見つけてから少し待ってコンパイルする
        if (settleTimer < 0.0f) return false;
        settleTimer -= dt;
        if (settleTimer > 0.0f) return false;
        settleTimer = -1.0f;
        return CompileAll() > 0;
    }

    int ShaderHotReload::CompileAll()
    {
        const auto start = std::chrono::steady_clock::now();

        // VS の HLSL コンパイラ（ビルド時）と同じく、Debug は最適化なし＋デバッグ情報、Release は最適化
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        // .hlsli は単独ではコンパイルしない（.hlsl から #include される）。
        // どの .hlsl がどの .hlsli を使っているかは調べず、毎回全部コンパイルし直す（全部で 1 秒もかからない）
        int ok = 0, failed = 0;
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path path = it->path();
            if (path.extension() != ".hlsl") continue;
            const std::string name = path.stem().string();
            const char* target = TargetFromName(name);
            if (!target) { Report("skipped (the name must end with VS or PS): " + path.string()); continue; }

            ComPtr<ID3DBlob> code, errors;
            const HRESULT hr = D3DCompileFromFile(path.wstring().c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "main", target, flags, 0, code.GetAddressOf(), errors.GetAddressOf());
            if (FAILED(hr)) {
                ++failed;
                Report("compile failed: " + name + "  (the previous shader is kept)\n" + BlobText(errors.Get()));
                continue;
            }
            if (errors) Report("warning: " + name + "\n" + BlobText(errors.Get()));   // 成功しても警告が入っていることがある

            const char* p = static_cast<const char*>(code->GetBufferPointer());
            SetShaderBytecodeOverride(name.c_str(), std::vector<char>(p, p + code->GetBufferSize()));
            ++ok;
        }

        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        Report("hot reload: " + std::to_string(ok) + " compiled, " + std::to_string(failed) + " failed (" + std::to_string(ms) + " ms)");
        return ok;
    }
}