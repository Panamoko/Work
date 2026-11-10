#pragma once
#include "Engine/Components/RoleComponent.h"

namespace uel {

    // Tank：前に出て、敵の攻撃を引き受ける。武器・アビリティは段階 3・6 で足す
    class TankComponent : public RoleComponent {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<TankComponent>(*this); }
        Role GetRole() const override { return Role::Tank; }
    };

} // namespace uel
