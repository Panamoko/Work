#include "Engine/Graphics/ShaderLoader.h"
#include "Engine/Config/EngineConfig.h"
#include <fstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // Data/Shader/<name>.cso
        std::string CsoPath(const char* name)
        {
            return std::string(kAssetRoot) + "/Shader/" + name + ".cso";
        }

        void Report(const std::string& message)
        {
            OutputDebugStringA(("[Shader] " + message + "\n").c_str());
        }
    }

    bool LoadShaderBytecode(const char* name, std::vector<char>& out)
    {
        const std::string path = CsoPath(name);
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);   // ateÅFç≈å„Ç…à⁄ìÆÇµÇƒëÂÇ´Ç≥ÇímÇÈ
        if (!ifs) {
            Report("not found: " + path + "  (build the project so that Shader/" + name + ".hlsl is compiled)");
            return false;
        }
        const std::streamsize size = ifs.tellg();
        if (size <= 0) { Report("empty file: " + path); return false; }

        out.resize(static_cast<size_t>(size));
        ifs.seekg(0, std::ios::beg);
        if (!ifs.read(out.data(), size)) { Report("read failed: " + path); return false; }
        return true;
    }

    bool CreateVertexShaderFromCso(ID3D11Device* device, const char* name,
        ComPtr<ID3D11VertexShader>& vs, std::vector<char>* bytecode)
    {
        std::vector<char> code;
        if (!LoadShaderBytecode(name, code)) return false;
        if (FAILED(device->CreateVertexShader(code.data(), code.size(), nullptr, vs.ReleaseAndGetAddressOf()))) {
            Report(std::string("CreateVertexShader failed: ") + name + "  (is its Shader Type set to Vertex?)");
            return false;
        }
        if (bytecode) *bytecode = std::move(code);
        return true;
    }

    bool CreatePixelShaderFromCso(ID3D11Device* device, const char* name, ComPtr<ID3D11PixelShader>& ps)
    {
        std::vector<char> code;
        if (!LoadShaderBytecode(name, code)) return false;
        if (FAILED(device->CreatePixelShader(code.data(), code.size(), nullptr, ps.ReleaseAndGetAddressOf()))) {
            Report(std::string("CreatePixelShader failed: ") + name + "  (is its Shader Type set to Pixel?)");
            return false;
        }
        return true;
    }

} // namespace uel
