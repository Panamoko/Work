#pragma once

#include <memory>
#include <string>
#include <vector>
#include "AI/BehaviorStatus.h"
#include "AI/BehaviorJudgment.h" // Š®‘S‚ÈŒ^‚É‚·‚éi•sŠ®‘SŒ^ delete ‚ð–h‚®j
#include "AI/BehaviorAction.h"

// How an intermediate node chooses among its children.
enum class SelectRule
{
    None,              // leaf node (no children)
    Priority,          // evaluate children by priority every frame; take the first
    // that does not fail (reactive -> higher priority can preempt)
    Sequence,          // run children in order; fail if any child fails; success when all pass
    SequentialLooping, // like Sequence, but restarts from the first child after the last
    Random,            // pick one runnable child at random and run it to completion
};

// One node of the behavior tree. Can be an intermediate (selects a child) or a
// leaf (owns an action). Judgment is an optional precondition.
class BehaviorNode
{
public:
    BehaviorNode(std::string name, int priority, SelectRule rule,
        std::unique_ptr<BehaviorJudgment> judgment,
        std::unique_ptr<BehaviorAction> action);
    ~BehaviorNode();

    const std::string& GetName() const { return name; }
    int GetPriority() const { return priority; }

    void AddChild(std::unique_ptr<BehaviorNode> child);
    BehaviorNode* FindNode(const std::string& target); // search self + descendants by name

    BehaviorStatus Tick(float elapsedTime); // run this node for one frame
    void Reset();                           // clear runtime progress (sequence step, etc.)

    bool CanEnter() const;                  // evaluate judgment (true if none)

private:
    BehaviorStatus TickLeaf(float dt);
    BehaviorStatus TickPriority(float dt);
    BehaviorStatus TickSequence(float dt, bool looping);
    BehaviorStatus TickRandom(float dt);

    std::string name;
    int priority = 0;
    SelectRule rule = SelectRule::None;
    std::unique_ptr<BehaviorJudgment> judgment;
    std::unique_ptr<BehaviorAction> action;
    std::vector<std::unique_ptr<BehaviorNode>> children;

    // runtime state
    int step = 0;               // current child index for Sequence
    int runningChild = -1;      // currently running child for Priority / Random
    bool actionStarted = false; // whether OnEnter() was already called for the leaf
};