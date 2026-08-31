#include "GameObject/Definition/SkeletonProfile.h"
#include "Graphics/Model/Model.h"
#include <vector>
#include <cctype>
#include <initializer_list>

void SkeletonProfile::Assign(SemanticBoneRole role, const std::string& nodeName)
{
	Entry& e = entries[static_cast<size_t>(role)];
	e.nodeName = nodeName;
	e.resolvedIndex = -1;
}

void SkeletonProfile::ResolveAgainst(const Model* model)
{
	for (Entry& e : entries)
		e.resolvedIndex = (model && !e.nodeName.empty()) ? model->GetNodeIndex(e.nodeName.c_str()) : -1;
}

int SkeletonProfile::ResolveIndex(SemanticBoneRole role) const
{
	return entries[static_cast<size_t>(role)].resolvedIndex;
}

bool SkeletonProfile::IsAssigned(SemanticBoneRole role) const
{
	return !entries[static_cast<size_t>(role)].nodeName.empty();
}

const std::string& SkeletonProfile::GetName(SemanticBoneRole role) const
{
	return entries[static_cast<size_t>(role)].nodeName;
}

SkeletonProfile SkeletonProfile::MakeGuess(const Model* model)
{
	SkeletonProfile sk;
	if (!model) return sk;

	// 完全一致の候補（すべて「例」）。最初に見つかったNodeを採用。
	auto tryExact = [&](SemanticBoneRole role, std::initializer_list<const char*> names)
		{
			for (const char* n : names)
				if (model->GetNodeIndex(n) >= 0) { sk.Assign(role, n); return; }
		};
	tryExact(SemanticBoneRole::PrimaryHand, { "Hand_R","hand_r","RightHand","mixamorig:RightHand","hand.R","wrist_r" });
	tryExact(SemanticBoneRole::SupportHand, { "Hand_L","hand_l","LeftHand","mixamorig:LeftHand","hand.L","wrist_l" });
	tryExact(SemanticBoneRole::Head, { "Head","head","mixamorig:Head" });
	tryExact(SemanticBoneRole::Hips, { "Hips","hips","Pelvis","mixamorig:Hips" });
	tryExact(SemanticBoneRole::Spine, { "Spine","spine","Chest","mixamorig:Spine" });
	tryExact(SemanticBoneRole::Root, { "Root","root","Armature" });

	// 完全一致で当たらない時は部分一致で救済（従来の hand/wrist/arm 探索相当）
	auto trySubstr = [&](SemanticBoneRole role, std::initializer_list<const char*> subs)
		{
			if (sk.IsAssigned(role)) return;
			const std::vector<Model::Node>& nodes = model->GetNodes();
			for (const char* sub : subs)
				for (const Model::Node& nd : nodes)
				{
					std::string low = nd.name;
					for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					if (low.find(sub) != std::string::npos) { sk.Assign(role, nd.name); return; }
				}
		};
	trySubstr(SemanticBoneRole::PrimaryHand, { "hand","wrist","arm" });

	sk.ResolveAgainst(model);
	return sk;
}