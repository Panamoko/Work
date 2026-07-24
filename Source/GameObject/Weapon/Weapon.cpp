#include "GameObject/Weapon/Weapon.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "Graphics/Model/Model.h"
#include <string>
#include <vector>
#include <cctype>
#include <cstring>

void Weapon::Bind(ModelComponent* modelComp, AnimatorComponent* animatorComp)
{
	model = modelComp;
	animator = animatorComp;
}

void Weapon::SetGripBoneName(const char* name)
{
	if (name) strncpy_s(gripBoneName, name, _TRUNCATE);
	else      gripBoneName[0] = '\0';
}

void Weapon::SetWorldTransform(const DirectX::XMFLOAT4X4& world)
{
	// AnimatorComponent が今フレームのポーズを submit した後に呼ぶこと。
	// pose のルートワールドを上書きして武器を配置する。
	if (model) model->ApplyWorldTransform(world);
}

int Weapon::GetBoneIndex(const char* boneName) const
{
	if (!model || !model->GetModel() || !boneName) return -1;
	return model->GetModel()->GetNodeIndex(boneName);
}

bool Weapon::GetBoneWorld(const char* boneName, DirectX::XMFLOAT4X4& outWorld) const
{
	if (!model || !model->GetModel()) return false;
	const std::vector<Model::Node>& nodes = model->GetPoseNodes();
	int idx = model->GetModel()->GetNodeIndex(boneName);
	if (idx < 0 || idx >= static_cast<int>(nodes.size())) return false;
	outWorld = nodes[idx].worldTransform;   // インスタンス毎のポーズから取得
	return true;
}

DirectX::XMFLOAT4X4 Weapon::GetGripPoint() const
{
	DirectX::XMFLOAT4X4 out;
	DirectX::XMStoreFloat4x4(&out, DirectX::XMMatrixIdentity());
	if (!model || !model->GetModel()) return out;

	const std::vector<Model::Node>& nodes = model->GetPoseNodes();
	if (nodes.empty()) return out;

	int idx = -1;
	if (gripBoneName[0] != '\0')
		idx = model->GetModel()->GetNodeIndex(gripBoneName);

	if (idx < 0)   // 自動検出
	{
		const char* keys[] = { "grip", "handle", "hand" };
		for (const char* k : keys)
		{
			for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
			{
				std::string low = nodes[i].name;
				for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
				if (low.find(k) != std::string::npos) { idx = i; break; }
			}
			if (idx >= 0) break;
		}
	}

	if (idx < 0 || idx >= static_cast<int>(nodes.size())) idx = 0; // フォールバック=武器ルート
	return nodes[idx].worldTransform;
}

void Weapon::PlayAnim(const char* animName, float fade, bool loop)
{
	if (!animator || !model || !model->GetModel()) return;
	int idx = model->GetModel()->GetAnimationIndex(animName);
	if (idx >= 0) animator->Play(idx, fade, loop);
}