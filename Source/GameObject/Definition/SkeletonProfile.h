#pragma once
#include <array>
#include <string>
#include "GameObject/Definition/SemanticRoles.h"

class Model;

// SemanticBoneRole -> node index（特定Model/Skeleton用）。
// 実Bone名は「例」。ここでRoleへMappingして名前差を吸収する（PrimaryHand等）。
class SkeletonProfile
{
public:
	void Assign(SemanticBoneRole role, const std::string& nodeName);
	void ResolveAgainst(const Model* model);

	int  ResolveIndex(SemanticBoneRole role) const;   // -1 = 未解決
	bool IsAssigned(SemanticBoneRole role) const;
	const std::string& GetName(SemanticBoneRole role) const;

	// 候補名/部分一致からの自動推定（実名固定しない。後でInspector/Sidecarで修正可）
	static SkeletonProfile MakeGuess(const Model* model);

private:
	struct Entry { std::string nodeName; int resolvedIndex = -1; };
	std::array<Entry, static_cast<size_t>(SemanticBoneRole::_Count)> entries{};
};