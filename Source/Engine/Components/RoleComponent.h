#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Game/Role.h"

namespace uel {

    // ロールの Component の共通の親（Tank / DPS / Support）。
    // ロールの Component は「そのロールで何ができるか」（武器の値・アビリティ）を持つ。「いつ使うか」は操作する側
    // （PlayerComponent / AllyComponent）が決める（0章「決めた設計」。UE の Gameplay Ability System の Ability に当たる）。
    // 今（1-4）は、どのロールかを返すだけの入れ物。武器の値は段階 3、アビリティは段階 6 で足す。
    // ロールを知りたいときは e->GetComponent<RoleComponent>() で、どのロールでも同じように取れる。
    class RoleComponent : public Component {
    public:
        virtual Role GetRole() const = 0;
    };

} // namespace uel
