#pragma once
#include "Engine/Components/RoleComponent.h"

namespace uel {

    // Support：味方の HP を回復する。武器・アビリティ（回復）は段階 2・3・6 で足す
    class SupportComponent : public RoleComponent {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<SupportComponent>(*this); }
        Role GetRole() const override { return Role::Support; }
    };

} // namespace uel
