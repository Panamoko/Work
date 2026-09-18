#pragma once

namespace uel {

    // データ専用のComponent基底。ロジックはSystemが持つ（仕様書5.3 / 20章）。
    class Component {
    public:
        virtual ~Component() = default;
    };

} // namespace uel