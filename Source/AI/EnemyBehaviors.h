#pragma once

#include "AI/BehaviorAction.h"
#include "AI/BehaviorJudgment.h"

class EnemyComponent;

// ---- Judgments ----

// True while the enemy currently sees the player.
class SightJudgment : public BehaviorJudgment
{
public:
    explicit SightJudgment(EnemyComponent* owner) : BehaviorJudgment(owner) {}
    bool Judgment() override;
};

// True while the enemy lost sight but is still within its search window.
class SearchJudgment : public BehaviorJudgment
{
public:
    explicit SearchJudgment(EnemyComponent* owner) : BehaviorJudgment(owner) {}
    bool Judgment() override;
};

// True when the player is within attack range and the cooldown is ready.
class AttackJudgment : public BehaviorJudgment
{
public:
    explicit AttackJudgment(EnemyComponent* owner) : BehaviorJudgment(owner) {}
    bool Judgment() override;
};

// ---- Actions ----

class WanderAction : public BehaviorAction
{
public:
    explicit WanderAction(EnemyComponent* owner) : BehaviorAction(owner) {}
    void OnEnter() override;
    BehaviorStatus Run(float elapsedTime) override;
};

class IdleAction : public BehaviorAction
{
public:
    explicit IdleAction(EnemyComponent* owner) : BehaviorAction(owner) {}
    void OnEnter() override;
    BehaviorStatus Run(float elapsedTime) override;
private:
    float timer = 0.0f;
};

// Approach the player and hold at stop distance (runs while the player is seen).
class ChaseAction : public BehaviorAction
{
public:
    explicit ChaseAction(EnemyComponent* owner) : BehaviorAction(owner) {}
    BehaviorStatus Run(float elapsedTime) override;
};

// Go to the last known position and look around (runs during the search window).
class SearchAction : public BehaviorAction
{
public:
    explicit SearchAction(EnemyComponent* owner) : BehaviorAction(owner) {}
    BehaviorStatus Run(float elapsedTime) override;
};

// Play the attack motion once (runs while in attack range).
class AttackAction : public BehaviorAction
{
public:
    explicit AttackAction(EnemyComponent* owner) : BehaviorAction(owner) {}
    void OnEnter() override;
    BehaviorStatus Run(float elapsedTime) override;
};