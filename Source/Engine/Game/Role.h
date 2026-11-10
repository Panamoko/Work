#pragma once
#include <cstdint>

namespace uel {

    // ロール（役割）。プレイヤーが1つ選び、残りの2つを AI 仲間が受け持つ（0章）。
    // 数値で保存・比較することがあるので、並び順を変えない（増やすときは末尾）。
    enum class Role : std::uint8_t { Tank, DPS, Support };

    constexpr int  kRoleCount = 3;
    constexpr Role kDefaultRole = Role::DPS;   // ロール選択を通らずに Stage を Play したときのロール

    // 画面に出す名前（英語のまま。ロール名はゲーム内の用語として扱う）
    inline const char* RoleName(Role r)
    {
        switch (r) {
        case Role::Tank:    return "Tank";
        case Role::DPS:     return "DPS";
        case Role::Support: return "Support";
        }
        return "?";
    }

    // ロール選択の画面に出す、短い説明（UTF-8）
    inline const char* RoleDescription(Role r)
    {
        switch (r) {
        case Role::Tank:    return u8"前に出て、敵の攻撃を引き受ける";
        case Role::DPS:     return u8"敵をすばやく倒す";
        case Role::Support: return u8"味方の HP を回復する";
        }
        return "";
    }

} // namespace uel
