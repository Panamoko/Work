#pragma once
namespace uel {
    struct InputState {
        bool  keyW = false, keyA = false, keyS = false, keyD = false;
        bool  jumpPressed = false;   // スペース（この瞬間）
        bool  shootClicked = false;   // 左クリック（この瞬間）
        bool  healHeld = false;       // 右クリックを押している間（マウスを固定しているときだけ。Support の回復。3-2）
        bool  lookActive = false;   // 視点を回してよい（マウスを Viewport に固定している間。3-1）
        float mouseDX = 0.0f, mouseDY = 0.0f;
        bool  enabled = false;        // 入力を受け付けるか
    };
}
