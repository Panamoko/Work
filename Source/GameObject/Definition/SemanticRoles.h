#pragma once
// ─────────────────────────────────────────────────────────────
//  名前非依存のためのロジカルロール定義（仕様書 §11 / §13）。
//  実Clip名・実Bone名・実Mesh名は「例」であり、これらのRoleへ
//  Mappingして吸収する。コードは Role でしか触らない。
// ─────────────────────────────────────────────────────────────

enum class LogicalAnimationRole
{
	Idle, MoveForward, MoveBackward, MoveLeft, MoveRight,
	JumpStart, FallLoop, Land,
	PrimaryAttack, SecondaryAttack, Reload,
	Ability1, Ability2, Ultimate, HitReact, Death,
	_Count
};

enum class MeshRole { Body, Head, Hair, Helmet, Armor, FirstPersonHidden, _Count };
enum class AttachmentRole { PrimaryHand, SupportHandGrip, BackHolster, Muzzle, Magazine, _Count };
enum class SemanticBoneRole { Root, Hips, Spine, Head, PrimaryHand, SupportHand, _Count };

// デバッグ表示 / ログ用（enum -> 文字列）。ヘッダ内inlineで完結。
inline const char* ToString(LogicalAnimationRole r)
{
	switch (r)
	{
	case LogicalAnimationRole::Idle:           return "Idle";
	case LogicalAnimationRole::MoveForward:    return "MoveForward";
	case LogicalAnimationRole::MoveBackward:   return "MoveBackward";
	case LogicalAnimationRole::MoveLeft:       return "MoveLeft";
	case LogicalAnimationRole::MoveRight:      return "MoveRight";
	case LogicalAnimationRole::JumpStart:      return "JumpStart";
	case LogicalAnimationRole::FallLoop:       return "FallLoop";
	case LogicalAnimationRole::Land:           return "Land";
	case LogicalAnimationRole::PrimaryAttack:  return "PrimaryAttack";
	case LogicalAnimationRole::SecondaryAttack:return "SecondaryAttack";
	case LogicalAnimationRole::Reload:         return "Reload";
	case LogicalAnimationRole::Ability1:       return "Ability1";
	case LogicalAnimationRole::Ability2:       return "Ability2";
	case LogicalAnimationRole::Ultimate:       return "Ultimate";
	case LogicalAnimationRole::HitReact:       return "HitReact";
	case LogicalAnimationRole::Death:          return "Death";
	default:                                   return "Unknown";
	}
}

inline const char* ToString(MeshRole r)
{
	switch (r)
	{
	case MeshRole::Body:              return "Body";
	case MeshRole::Head:              return "Head";
	case MeshRole::Hair:              return "Hair";
	case MeshRole::Helmet:            return "Helmet";
	case MeshRole::Armor:             return "Armor";
	case MeshRole::FirstPersonHidden: return "FirstPersonHidden";
	default:                          return "Unknown";
	}
}

inline const char* ToString(AttachmentRole r)
{
	switch (r)
	{
	case AttachmentRole::PrimaryHand:     return "PrimaryHand";
	case AttachmentRole::SupportHandGrip: return "SupportHandGrip";
	case AttachmentRole::BackHolster:     return "BackHolster";
	case AttachmentRole::Muzzle:          return "Muzzle";
	case AttachmentRole::Magazine:        return "Magazine";
	default:                              return "Unknown";
	}
}

inline const char* ToString(SemanticBoneRole r)
{
	switch (r)
	{
	case SemanticBoneRole::Root:        return "Root";
	case SemanticBoneRole::Hips:        return "Hips";
	case SemanticBoneRole::Spine:       return "Spine";
	case SemanticBoneRole::Head:        return "Head";
	case SemanticBoneRole::PrimaryHand: return "PrimaryHand";
	case SemanticBoneRole::SupportHand: return "SupportHand";
	default:                            return "Unknown";
	}
}