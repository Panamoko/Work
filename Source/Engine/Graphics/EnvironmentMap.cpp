#include "Engine/Graphics/EnvironmentMap.h"
#include "Engine/Graphics/DdsTexture.h"
#include "Engine/Config/EngineConfig.h"
#include <string>

namespace uel {
    namespace {
        // 使う景色のフォルダ（Data/Sprite/ の下）。中身は skybox.dds などの同じ名前のファイル一式
        //   "IBL_PureSky" : 曇り空（地面は平ら。建物や木が無い）
        //   "IBL"         : 野原の写真（木や花が写っている）
        constexpr const char* kIblFolder = "IBL_PureSky";

        // Data/Sprite/<kIblFolder>/<name>
        std::string IblPath(const char* name)
        {
            return std::string(kAssetRoot) + "/Sprite/" + kIblFolder + "/" + name;
        }

        // DDS を読む。wantCube と形（キューブマップか 2D か）が違ったら使わない
        bool LoadTexture(ID3D11Device* device, const char* name, bool wantCube,
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
        {
            const std::string path = IblPath(name);
            DdsTextureInfo info;
            if (!LoadDdsTexture(device, path.c_str(), srv, &info)) return false;
            if (info.isCube != wantCube) {
                OutputDebugStringA(("[DDS] " + std::string(wantCube ? "not a cube map: " : "not a 2D texture: ") + path + "\n").c_str());
                srv.Reset();
                return false;
            }
            return true;
        }
    }

    bool EnvironmentMap::Initialize(ID3D11Device* device)
    {
        // 1つ失敗しても、残りは読む（読めたものだけ使う）
        const bool okSky = LoadTexture(device, "skybox.dds", true, sky);
        const bool okSpecular = LoadTexture(device, "specular_pmrem.dds", true, specular);
        const bool okLut = LoadTexture(device, "lut_ggx.dds", false, brdfLut);
        return okSky && okSpecular && okLut;
    }
}