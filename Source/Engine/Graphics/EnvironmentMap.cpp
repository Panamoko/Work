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

        // キューブマップを読む。2D の画像だったら使わない
        bool LoadCube(ID3D11Device* device, const char* name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
        {
            const std::string path = IblPath(name);
            DdsTextureInfo info;
            if (!LoadDdsTexture(device, path.c_str(), srv, &info)) return false;
            if (!info.isCube) {
                OutputDebugStringA(("[DDS] not a cube map: " + path + "\n").c_str());
                srv.Reset();
                return false;
            }
            return true;
        }
    }

    bool EnvironmentMap::Initialize(ID3D11Device* device)
    {
        return LoadCube(device, "skybox.dds", sky);
    }
}