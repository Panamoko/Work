#pragma once
#include "Engine/Components/Component.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

namespace uel {

    enum class MeshRole : uint8_t { None = 0, FirstPersonHidden = 1 };

    class MeshRendererComponent : public Component {
    public:

        std::unique_ptr<Component> Clone() const override { return std::make_unique<MeshRendererComponent>(*this); }

        std::string       assetId;
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool              visible = true;

        std::vector<uint8_t> hiddenSubmesh;  // index=submesh、非0で手動非表示
        std::vector<uint8_t> submeshRole;    // index=submesh、MeshRole値
        // index=submesh、非0で「当たり判定なし」（5-4b。保存する。版 27）。見た目はそのまま、地面・壁・弾の線に当たらない。
        // 草・花・海など（UE の「No Collision」）。GroundQuery の RaycastGroundColumn / RaycastMesh に渡す。空なら全部に当たる
        std::vector<uint8_t> noCollisionSubmesh;
        bool                 firstPerson = false; // ON で FirstPersonHidden を隠す
    };

} // namespace uel