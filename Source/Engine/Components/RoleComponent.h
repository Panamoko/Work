#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Game/Role.h"
#include "Engine/Game/Weapon.h"

namespace uel {

    // ロールの Component の共通の親（Tank / DPS / Support）。
    // ロールの Component は「そのロールで何ができるか」（武器の値・アビリティ）を持つ。「いつ使うか」は操作する側
    // （PlayerComponent / AllyComponent）が決める（0章「決めた設計」。UE の Gameplay Ability System の Ability に当たる）。
    // ロールを知りたいときは e->GetComponent<RoleComponent>() で、どのロールでも同じように取れる。
    //
    // 武器（3-3）：値はロールごとの表（Game/Weapon の WeaponForRole）から取り、弾の残りなどの状態（weaponState）はここに持つ。
    // アビリティは段階 6 で足す。
    class RoleComponent : public Component {
    public:
        virtual Role GetRole() const = 0;

        // このロールの武器の値（表の1か所にまとめてある。ロールの Component ごとに値を持たない）
        const WeaponStats& GetWeapon() const { return WeaponForRole(GetRole()); }

        // 武器の今の状態（弾の残り・リロード・次に撃てるまで）。Play 中だけの値（ロールの Component は保存しない）
        WeaponState weaponState;
    };

} // namespace uel
