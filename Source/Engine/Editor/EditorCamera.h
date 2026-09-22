#pragma once
#include <DirectXMath.h>

namespace uel {

    // UEライクなフライEditorカメラ。
    // 右クリック中：マウスで回転／WASDで移動／Q・Eで上下／ホイールで前後。
    class EditorCamera {
    public:
        void Update(bool viewportHovered);          // 毎フレーム（ImGui io を使用）
        DirectX::XMFLOAT4X4 GetView() const;

    private:
        DirectX::XMFLOAT3 ForwardVector() const;

        DirectX::XMFLOAT3 position{ 4.0f, 4.0f, -7.0f };
        float yaw = -0.5f;
        float pitch = -0.4f;
        float moveSpeed = 6.0f;
        float lookSens = 0.004f;
        bool  controlling = false;
    };

} // namespace uel