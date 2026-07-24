#pragma once
#include <DirectXMath.h>

class ModelComponent;
class AnimatorComponent;

// ─────────────────────────────────────────────────────────────
//  Weapon : 1挺のFPS武器インスタンス
//  ・Actorは持たない。武器Actorの ModelComponent(武器.glb) と
//    AnimatorComponent に「バインド」して使う軽量ラッパ。
//  ・武器ローカルの子ボーン(Magazine / Bolt / Trigger / Muzzle …)を
//    ワールド空間で取り出せる → リロード演出・IK・マズルVFXに使う。
// ─────────────────────────────────────────────────────────────
class Weapon
{
public:
	Weapon() = default;

	void Bind(ModelComponent* model, AnimatorComponent* animator);
	bool IsValid() const { return model != nullptr; }

	// オフセットスタック合成後の最終ワールドを武器へ流し込む
	void SetWorldTransform(const DirectX::XMFLOAT4X4& world);

	// ── ボーンアクセス（武器ローカルの部品）──
	int  GetBoneIndex(const char* boneName) const;
	bool GetBoneWorld(const char* boneName, DirectX::XMFLOAT4X4& outWorld) const;

	// ── ⑫ グリップ点（IK対応）──
	// 武器の握り基準点をワールドで返す。カメラ相対(ビューモデル)駆動時は
	// 「手をここへ寄せろ」という腕IKのターゲットとして使える。
	DirectX::XMFLOAT4X4 GetGripPoint() const;
	void SetGripBoneName(const char* name);

	// ── アニメーション委譲 ──
	void PlayAnim(const char* animName, float fade = 0.1f, bool loop = true);

	ModelComponent* GetModelComponent() const { return model; }
	AnimatorComponent* GetAnimator()       const { return animator; }

private:
	ModelComponent* model = nullptr;
	AnimatorComponent* animator = nullptr;
	char gripBoneName[64] = "";   // 空 = Grip/Handle/Hand を自動検出
};