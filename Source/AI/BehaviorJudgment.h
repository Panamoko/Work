#pragma once

class EnemyComponent;

// Precondition attached to a node: "may this node run right now?"
// e.g. can I see the player, am I in attack range, is my HP low, ...
class BehaviorJudgment
{
public:
    explicit BehaviorJudgment(EnemyComponent* owner) : owner(owner) {}
    virtual ~BehaviorJudgment() = default;

    virtual bool Judgment() = 0;

protected:
    EnemyComponent* owner;
};