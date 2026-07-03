#pragma once

#include "AI/BehaviorStatus.h"

class EnemyComponent;

// Leaf behavior. Run() is called every frame and returns Running while the
// action is still playing out (so e.g. an attack animation is not cut off).
class BehaviorAction
{
public:
    explicit BehaviorAction(EnemyComponent* owner) : owner(owner) {}
    virtual ~BehaviorAction() = default;

    // Called once when the action (re)starts, before the first Run().
    virtual void OnEnter() {}
    virtual BehaviorStatus Run(float elapsedTime) = 0;

protected:
    EnemyComponent* owner;
};