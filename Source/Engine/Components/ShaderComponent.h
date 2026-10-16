#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Graphics/ShadingParams.h"


namespace uel {

    // この Entity の MeshRenderer / SkinnedMesh を「どう描くか」を決める。
    // 付いていない Entity は既定値（Lit・影を受ける）で描かれるので、今までと同じ見た目になる。
    class ShaderComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<ShaderComponent>(*this); }

        ShadingParams params;
    };

} // namespace uel