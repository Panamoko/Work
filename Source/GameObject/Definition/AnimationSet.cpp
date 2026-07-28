#include "GameObject/Definition/AnimationSet.h"
#include "Graphics/Model/Model.h"

void AnimationSet::Assign(LogicalAnimationRole role, const std::string& clipName,
	bool loop, float blendTime, float playbackRate, LogicalAnimationRole fallback)
{
	AnimationRoleEntry& e = entries[static_cast<size_t>(role)];
	e.clipName = clipName;
	e.loop = loop;
	e.blendTime = blendTime;
	e.playbackRate = playbackRate;
	e.fallback = fallback;
	e.resolvedIndex = -1;   // 次のResolveAgainstで解決
}

void AnimationSet::ResolveAgainst(const Model* model)
{
	for (AnimationRoleEntry& e : entries)
	{
		e.resolvedIndex = (model && !e.clipName.empty())
			? model->GetAnimationIndex(e.clipName.c_str())
			: -1;
	}
}

int AnimationSet::ResolveIndex(LogicalAnimationRole role) const
{
	LogicalAnimationRole cur = role;
	// Fallback連鎖を辿る（自己参照 or _Count回でループ防止）
	for (int guard = 0; guard < static_cast<int>(LogicalAnimationRole::_Count); ++guard)
	{
		const AnimationRoleEntry& e = entries[static_cast<size_t>(cur)];
		if (e.resolvedIndex >= 0) return e.resolvedIndex;
		if (e.fallback == cur)    break;          // 自己Fallback=これ以上たどらない
		cur = e.fallback;
	}
	return -1;
}

const AnimationRoleEntry* AnimationSet::Find(LogicalAnimationRole role) const
{
	return &entries[static_cast<size_t>(role)];
}

bool AnimationSet::IsAssigned(LogicalAnimationRole role) const
{
	return !entries[static_cast<size_t>(role)].clipName.empty();
}

AnimationSet AnimationSet::MakeGuess(const Model* model)
{
	AnimationSet set;
	// 候補名はすべて「例」。最初に一致したClipを採用する（実名固定ではない）。
	auto tryAssign = [&](LogicalAnimationRole role, std::initializer_list<const char*> names, bool loop)
		{
			if (!model) return;
			for (const char* n : names)
			{
				if (model->GetAnimationIndex(n) >= 0) { set.Assign(role, n, loop); return; }
			}
		};

	tryAssign(LogicalAnimationRole::Idle, { "IDLE", "Idle", "idle", "Rifle_Idle", "Pistol_IDLE" }, true);
	tryAssign(LogicalAnimationRole::MoveForward, { "RUN", "Run", "run", "WALK", "Walk", "Rifle_Run", "Pistol_WALK" }, true);
	tryAssign(LogicalAnimationRole::JumpStart, { "JUMP", "Jump", "Pistol_JUMP_START" }, false);
	tryAssign(LogicalAnimationRole::PrimaryAttack, { "PUNCHRIGHT", "PUNCHLEFT", "Attack", "Rifle_Fire", "Pistol_FIRE" }, false);
	tryAssign(LogicalAnimationRole::Reload, { "RELOAD", "Reload", "Rifle_Reload", "Pistol_RELOAD" }, false);
	tryAssign(LogicalAnimationRole::Death, { "DEATH", "Death", "die", "Rifle_Death" }, false);

	set.ResolveAgainst(model);
	return set;
}