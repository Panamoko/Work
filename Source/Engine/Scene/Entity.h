#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Engine/Scene/EntityId.h"
#include "Engine/Components/Component.h"

namespace uel {

    class Entity {
    public:
        EntityId id;
        std::string name;
        EntityId parent;                          // –³Œø‚È‚çe‚È‚µ
        std::vector<EntityId> children;
        std::vector<std::unique_ptr<Component>> components;

        template<class T, class... Args>
        T* AddComponent(Args&&... args) {
            if (T* existing = GetComponent<T>()) return existing; // “¯Œ^‚Ìd•¡‚ğ–h‚®
            auto up = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = up.get();
            components.push_back(std::move(up));
            return raw;
        }

        template<class T>
        T* GetComponent() const {
            for (const auto& c : components)
                if (T* t = dynamic_cast<T*>(c.get())) return t;
            return nullptr;
        }

        template<class T>
        void RemoveComponent() {
            for (auto it = components.begin(); it != components.end(); ++it)
                if (dynamic_cast<T*>(it->get())) { components.erase(it); return; }
        }

    };

} // namespace uel