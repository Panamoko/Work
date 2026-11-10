#pragma once
#include "Engine/Components/RoleComponent.h"

namespace uel {

    // DPS：敵をすばやく倒す。武器・アビリティは段階 3・6 で足す
    class DPSComponent : public RoleComponent {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<DPSComponent>(*this); }
        Role GetRole() const override { return Role::DPS; }
    };

} // namespace uel
