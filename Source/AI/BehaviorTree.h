#pragma once

#include <memory>
#include <string>
#include "AI/BehaviorNode.h" // SelectRule, BehaviorNode

class BehaviorJudgment;
class BehaviorAction;

// The behavior tree owned by an enemy. Build it with AddNode(), tick it with Update().
class BehaviorTree
{
public:
    // parentName == "" registers the root.
    // A leaf node has rule == None and a non-null action.
    void AddNode(const std::string& parentName, const std::string& nodeName,
        int priority, SelectRule rule,
        std::unique_ptr<BehaviorJudgment> judgment,
        std::unique_ptr<BehaviorAction> action);

    void Update(float elapsedTime);

    bool IsBuilt() const { return root != nullptr; }

private:
    std::unique_ptr<BehaviorNode> root;
};