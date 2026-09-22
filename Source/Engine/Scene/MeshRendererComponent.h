#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

namespace uel {

    enum class MeshRole : uint8_t { None = 0, FirstPersonHidden = 1 };

    class MeshRendererComponent : public Component {
    public:
        std::string       assetId;
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool              visible = true;

        std::vector<uint8_t> hiddenSubmesh;  // index=submesh、非0で手動非表示
        std::vector<uint8_t> submeshRole;    // index=submesh、MeshRole値
        bool                 firstPerson = false; // ON で FirstPersonHidden を隠す
    };

} // namespace uel