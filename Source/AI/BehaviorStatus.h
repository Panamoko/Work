#pragma once

// Result of ticking a behavior-tree node (same 3 values as Unreal/Unity BTs).
enum class BehaviorStatus
{
    Success, // finished successfully
    Failure, // could not run / aborted
    Running, // still in progress; tick again next frame
};