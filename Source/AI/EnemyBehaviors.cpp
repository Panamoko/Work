#include "AI/EnemyBehaviors.h"
#include "GameObject/Components/EnemyComponent.h"

// ---- Judgments ----

bool SightJudgment::Judgment() { return owner->IsPlayerVisible(); }
bool SearchJudgment::Judgment() { return owner->IsSearching(); }
bool AttackJudgment::Judgment() { return owner->CanAttack(); }

// ---- Actions ----

void WanderAction::OnEnter()
{
    owner->SetWanderTarget();
    owner->PlayWalk();
}

BehaviorStatus WanderAction::Run(float elapsedTime)
{
    return owner->MoveToTarget(elapsedTime) ? BehaviorStatus::Success : BehaviorStatus::Running;
}

void IdleAction::OnEnter()
{
    owner->PlayIdle();
    timer = owner->NextIdleDuration();
}

BehaviorStatus IdleAction::Run(float elapsedTime)
{
    timer -= elapsedTime;
    return (timer <= 0.0f) ? BehaviorStatus::Success : BehaviorStatus::Running;
}

BehaviorStatus ChaseAction::Run(float elapsedTime)
{
    owner->ChaseUpdate(elapsedTime);
    return BehaviorStatus::Running;
}

BehaviorStatus SearchAction::Run(float elapsedTime)
{
    owner->SearchUpdate(elapsedTime);
    return BehaviorStatus::Running;
}

void AttackAction::OnEnter()
{
    owner->BeginAttack();
}

BehaviorStatus AttackAction::Run(float elapsedTime)
{
    // 攻撃モーションが1回終わったら Success（→クールダウン中は Chase が待機）
    return owner->AttackUpdate(elapsedTime) ? BehaviorStatus::Success : BehaviorStatus::Running;
}