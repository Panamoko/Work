#pragma once
#include <string>

namespace uel {
    // Play中のScene遷移と結果の受け渡し（UEのGameInstance的な役割）
    struct GameFlow {
        std::string requestedScene;   // 空でなければ次フレームでこのSceneへ遷移
        std::string lastResult;       // "CLEAR" / "GAME OVER"
        float       lastTime = 0.0f;  // 決着時のプレイ時間
        float       playTime = 0.0f;  // 現在Sceneの経過時間
    };
}