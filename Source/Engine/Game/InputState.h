#pragma once
namespace uel {
    struct InputState {
        bool  keyW = false, keyA = false, keyS = false, keyD = false;
        bool  jumpPressed = false;   // スペース（この瞬間）
        bool  shootClicked = false;   // 左クリック（この瞬間）
        bool  lookActive = false;   // 右クリック中
        float mouseDX = 0.0f, mouseDY = 0.0f;
        bool  enabled = false;        // 入力を受け付けるか
    };
}