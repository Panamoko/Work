#pragma once
#include <string>
#include "Engine/Game/Role.h"

namespace uel {
    // Play中のScene遷移と結果の受け渡し（UEのGameInstance的な役割）
    struct GameFlow {
        std::string requestedScene;   // 空でなければ次フレームでこのSceneへ遷移
        std::string lastResult;       // "CLEAR" / "GAME OVER"
        float       lastTime = 0.0f;  // 決着時のプレイ時間
        float       playTime = 0.0f;  // 現在Sceneの経過時間
        Role        playerRole = kDefaultRole;   // プレイヤーが選んだロール（ロール選択の画面で書き、Stage が読む）。Play を始めるたびに既定に戻す
    };
}
