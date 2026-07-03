#pragma once

#include "GameObject/Component.h"

// 汎用HP（体力）コンポーネント。ダメージを受ける対象に付ける。
class HealthComponent : public Component
{
public:
	explicit HealthComponent(float maxHp = 100.0f) : hp(maxHp), maxHp(maxHp) {}

	const char* GetName() const override { return "Health"; }
	void OnGUI() override;

	void ApplyDamage(float amount);
	void Heal(float amount);

	float GetHp() const { return hp; }
	float GetMaxHp() const { return maxHp; }
	float GetRatio() const { return (maxHp > 0.0f) ? hp / maxHp : 0.0f; }
	bool  IsDead() const { return hp <= 0.0f; }

private:
	float hp = 100.0f;
	float maxHp = 100.0f;
};