#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

namespace uel {

    // スキン付きメッシュ（どのモデルを出すか）と、その「いまの姿勢」を持つ。
    // 姿勢を決めるのは AnimationSystem。描画（RenderSystem）は、ここに入っている結果を読むだけ。
    class SkinnedMeshComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<SkinnedMeshComponent>(*this); }

        std::string       assetPath;              // スキン付き .glb
        std::string       animPath;   // アニメ別ファイル(rig glb)。空ならassetPath内のアニメ
        int               clipIndex = 0;
        float             time = 0.0f;
        float             speed = 1.0f;
        bool              playing = true;
        bool              loop = true;
        bool              visible = true;
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

        // 一人称のときに潰す骨の名前（頭など）。空なら何もしない。
        // カメラが頭の位置にあると、自分の頭や髪が画面をふさぐので、その骨と子の骨を1点に縮めて見えなくする。
        std::string       fpHideBone;

        // ---- 実行時だけの値（保存しない）----
        bool firstPerson = false;                      // 一人称で描くか。Play を始めるとき、プレイヤーだけ true になる
        // AnimationSystem が毎フレーム書き込む
        std::vector<DirectX::XMFLOAT4X4> boneGlobal;   // ボーンごとのモデル空間の行列（skeleton.bones と同じ並び）
        std::vector<DirectX::XMFLOAT4X4> palette;      // スキニング用の行列（skeleton.joints と同じ並び）
    };

} // namespace uel