#pragma once
#include <string>
#include <cstdint>
#include "Engine/Game/Role.h"

namespace uel {
    // ゲームオーバーの理由（5-5a。Result の画面に出す）。並び順を変えない（増やすときは末尾）
    enum class LoseReason : std::uint8_t {
        None,           // 負けていない
        PlayerDead,     // プレイヤーが倒れた（ダウンのまま時間切れ。ダウンの仕組みの無い Scene では HP 0）
        PartyDown,      // 3人とも戦えなくなった（ダウンか Dead）
    };

    // Play中のScene遷移と結果の受け渡し（UEのGameInstance的な役割）
    struct GameFlow {
        std::string requestedScene;   // 空でなければ次フレームでこのSceneへ遷移
        std::string lastResult;       // "CLEAR" / "GAME OVER"
        float       lastTime = 0.0f;  // 決着時のプレイ時間
        float       playTime = 0.0f;  // 現在Sceneの経過時間
        Role        playerRole = kDefaultRole;   // プレイヤーが選んだロール（ロール選択の画面で書き、Stage が読む）。Play を始めるたびに既定に戻す

        // ---- ステージの結果（5-5a）----
        // GameRule が決着したときに lastResult と一緒に書き、Result の画面が読む（UE の GameState の試合結果に当たる）。
        // lastResult が空のあいだは使わない
        Role        lastRole = kDefaultRole;            // そのステージで使ったロール
        int         lastStopsReached = 0;               // 案内ロボットが着いた止まる場所の数
        int         lastStopCount = 0;                  // 止まる場所の数（0 = 案内ロボットのいない Scene。到達の行は出さない）
        bool        lastBossDefeated = false;           // ボスを倒してクリアしたか
        LoseReason  lastLoseReason = LoseReason::None;  // ゲームオーバーの理由（クリアなら None）
    };
}
