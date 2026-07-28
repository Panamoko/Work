#pragma once
#include <array>
#include <string>
#include <initializer_list>
#include "GameObject/Definition/SemanticRoles.h"

class Model;

// Role単位の再生設定。clipNameは「例」であり、データ側で与える。
struct AnimationRoleEntry
{
	std::string          clipName;                        // 実Clip名（例。空=未割当）
	bool                 loop = true;
	float                blendTime = 0.15f;
	float                playbackRate = 1.0f;
	LogicalAnimationRole fallback = LogicalAnimationRole::Idle;
	int                  resolvedIndex = -1;               // 特定Modelへ解決したClip index（-1=未解決）
};

// あるModel（1キャラ/1Skeleton）に対する Role -> Clip 対応表。
// 実Clip名はここに閉じ込め、上位コードは Role だけで再生する。
class AnimationSet
{
public:
	// Role へ実Clip名（例）を割り当てる（Sidecar/JSONから流し込む想定）
	void Assign(LogicalAnimationRole role, const std::string& clipName,
		bool loop = true, float blendTime = 0.15f, float playbackRate = 1.0f,
		LogicalAnimationRole fallback = LogicalAnimationRole::Idle);

	// Model の実Clipへ名前解決（Import後 / モデル差し替え後に1回）
	void ResolveAgainst(const Model* model);

	// Role -> 解決済みindex（Fallback連鎖を辿る）。見つからなければ -1
	int  ResolveIndex(LogicalAnimationRole role) const;

	const AnimationRoleEntry* Find(LogicalAnimationRole role) const;
	bool IsAssigned(LogicalAnimationRole role) const;

	// よくある命名の候補（すべて「例」）を試す自動推定。実名は固定しない。
	// 推定結果は後でInspector/Sidecarで修正できる前提。
	static AnimationSet MakeGuess(const Model* model);

private:
	std::array<AnimationRoleEntry, static_cast<size_t>(LogicalAnimationRole::_Count)> entries{};
};