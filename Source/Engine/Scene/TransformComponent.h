#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>

namespace uel {

    class TransformComponent : public Component {
    public:
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 rotationEuler{ 0.0f, 0.0f, 0.0f }; // “x
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

        DirectX::XMFLOAT4X4 GetWorldMatrix() const {
            using namespace DirectX;
            XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
            XMMATRIX R = XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(rotationEuler.x),
                XMConvertToRadians(rotationEuler.y),
                XMConvertToRadians(rotationEuler.z));
            XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
            XMFLOAT4X4 out;
            XMStoreFloat4x4(&out, S * R * T);
            return out;
        }
    };

} // namespace uel