#include "AI/BehaviorNode.h"
#include "AI/BehaviorJudgment.h"
#include "AI/BehaviorAction.h"
#include <algorithm>
#include <random>

BehaviorNode::BehaviorNode(std::string name, int priority, SelectRule rule,
    std::unique_ptr<BehaviorJudgment> judgment,
    std::unique_ptr<BehaviorAction> action)
    : name(std::move(name)), priority(priority), rule(rule),
    judgment(std::move(judgment)), action(std::move(action))
{
}

BehaviorNode::~BehaviorNode() = default;

void BehaviorNode::AddChild(std::unique_ptr<BehaviorNode> child)
{
    children.push_back(std::move(child));
}

BehaviorNode* BehaviorNode::FindNode(const std::string& target)
{
    if (name == target) return this;
    for (auto& c : children)
    {
        if (BehaviorNode* found = c->FindNode(target)) return found;
    }
    return nullptr;
}

bool BehaviorNode::CanEnter() const
{
    return judgment == nullptr || judgment->Judgment();
}

BehaviorStatus BehaviorNode::Tick(float dt)
{
    // Precondition: if it fails, this whole node fails and resets.
    if (!CanEnter())
    {
        Reset();
        return BehaviorStatus::Failure;
    }

    if (action) return TickLeaf(dt); // leaf

    switch (rule)
    {
    case SelectRule::Priority:          return TickPriority(dt);
    case SelectRule::Sequence:          return TickSequence(dt, false);
    case SelectRule::SequentialLooping: return TickSequence(dt, true);
    case SelectRule::Random:            return TickRandom(dt);
    default:                            return BehaviorStatus::Failure;
    }
}

void BehaviorNode::Reset()
{
    step = 0;
    runningChild = -1;
    actionStarted = false;
    for (auto& c : children) c->Reset();
}

BehaviorStatus BehaviorNode::TickLeaf(float dt)
{
    if (!actionStarted)
    {
        action->OnEnter();
        actionStarted = true;
    }
    BehaviorStatus st = action->Run(dt);
    if (st != BehaviorStatus::Running) actionStarted = false;
    return st;
}

BehaviorStatus BehaviorNode::TickPriority(float dt)
{
    // Evaluate children in ascending priority (smaller number = higher priority),
    // re-checked every frame so a higher-priority branch can preempt a lower one.
    std::vector<int> order(children.size());
    for (int i = 0; i < static_cast<int>(children.size()); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
        [this](int a, int b) { return children[a]->priority < children[b]->priority; });

    for (int idx : order)
    {
        BehaviorStatus st = children[idx]->Tick(dt);
        if (st == BehaviorStatus::Failure) continue; // try the next priority

        // Selected this child. If we switched away from another running child, reset it.
        if (runningChild >= 0 && runningChild != idx) children[runningChild]->Reset();
        runningChild = (st == BehaviorStatus::Running) ? idx : -1;
        return st;
    }

    runningChild = -1;
    return BehaviorStatus::Failure;
}

BehaviorStatus BehaviorNode::TickSequence(float dt, bool looping)
{
    if (children.empty()) return BehaviorStatus::Success;

    while (step < static_cast<int>(children.size()))
    {
        BehaviorStatus st = children[step]->Tick(dt);
        if (st == BehaviorStatus::Running) return BehaviorStatus::Running;
        if (st == BehaviorStatus::Failure) { step = 0; return BehaviorStatus::Failure; }
        ++step; // child succeeded -> advance
    }

    step = 0;
    // Looping sequences stay "Running" so the parent keeps selecting them (e.g. patrol).
    return looping ? BehaviorStatus::Running : BehaviorStatus::Success;
}

BehaviorStatus BehaviorNode::TickRandom(float dt)
{
    if (runningChild < 0)
    {
        std::vector<int> candidates;
        for (int i = 0; i < static_cast<int>(children.size()); ++i)
        {
            if (children[i]->CanEnter()) candidates.push_back(i);
        }
        if (candidates.empty()) return BehaviorStatus::Failure;

        static std::mt19937 rng{ std::random_device{}() };
        runningChild = candidates[rng() % candidates.size()];
    }

    BehaviorStatus st = children[runningChild]->Tick(dt);
    if (st != BehaviorStatus::Running) runningChild = -1;
    return st;
}