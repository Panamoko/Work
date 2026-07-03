#include <imgui.h>
#include "GameObject/Components/HealthComponent.h"

void HealthComponent::ApplyDamage(float amount)
{
	if (hp <= 0.0f) return;
	hp -= amount;
	if (hp < 0.0f) hp = 0.0f;
}

void HealthComponent::Heal(float amount)
{
	hp += amount;
	if (hp > maxHp) hp = maxHp;
}

void HealthComponent::OnGUI()
{
	ImVec4 col = IsDead()
		? ImVec4(0.6f, 0.6f, 0.6f, 1.0f)
		: (GetRatio() < 0.3f ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
	ImGui::ProgressBar(GetRatio(), ImVec2(-1, 0));
	ImGui::PopStyleColor();
	ImGui::Text("HP: %.0f / %.0f%s", hp, maxHp, IsDead() ? "   (DEAD)" : "");
}