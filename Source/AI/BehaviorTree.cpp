#include "AI/BehaviorTree.h"
#include "AI/BehaviorJudgment.h"
#include "AI/BehaviorAction.h"
#include <cassert>

void BehaviorTree::AddNode(const std::string& parentName, const std::string& nodeName,
    int priority, SelectRule rule,
    std::unique_ptr<BehaviorJudgment> judgment,
    std::unique_ptr<BehaviorAction> action)
{
    auto node = std::make_unique<BehaviorNode>(
        nodeName, priority, rule, std::move(judgment), std::move(action));

    if (parentName.empty())
    {
        root = std::move(node); // register as root
        return;
    }

    BehaviorNode* parent = root ? root->FindNode(parentName) : nullptr;
    assert(parent && "BehaviorTree::AddNode - parent node not found");
    if (parent) parent->AddChild(std::move(node));
}

void BehaviorTree::Update(float elapsedTime)
{
    if (root) root->Tick(elapsedTime);
}