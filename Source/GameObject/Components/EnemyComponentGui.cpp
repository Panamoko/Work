#include <imgui.h>
#include <fstream>
#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>
#include <random>
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/HealthComponent.h"
#include "GameObject/Actor.h"
#include "AI/EnemyBehaviors.h"
#include "Physics/CollisionMesh.h"
#include "AI/NavGrid.h"

// ---- Inspector ----
void EnemyComponent::OnGUI()
{
	ImGui::Text("Enemy  (%s)", dead ? "DEAD" : (state == State::Walk ? "WALK" : "IDLE"));
	ImGui::ProgressBar(maxHp > 0.0f ? hp / maxHp : 0.0f, ImVec2(-1, 0));
	ImGui::Text("HP: %.0f / %.0f", hp, maxHp);

	if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Detect Range", &detectRange, 1.0f, 50.0f);
		ImGui::SliderFloat("Stop Distance", &stopDistance, 0.5f, 10.0f);
		ImGui::SliderFloat("Move Speed", &moveSpeed, 0.0f, 10.0f);
		ImGui::SliderFloat("Turn Speed", &turnSpeed, 1.0f, 20.0f);
		ImGui::SliderFloat("Blend Time", &blendTime, 0.0f, 0.6f);
	}

	if (ImGui::CollapsingHeader("Patrol"))
	{
		ImGui::SliderFloat("Wander Radius", &wanderRadius, 1.0f, 30.0f);
		ImGui::SliderFloat("Arrive Radius", &arriveRadius, 0.1f, 3.0f);
		ImGui::SliderFloat("Idle Min", &idleMin, 0.0f, 5.0f);
		ImGui::SliderFloat("Idle Max", &idleMax, 0.0f, 8.0f);
	}

	if (ImGui::CollapsingHeader("Perception"))
	{
		ImGui::SliderFloat("FOV (deg)", &fovDegrees, 20.0f, 180.0f);
		ImGui::SliderFloat("Eye Height", &eyeHeight, 0.0f, 3.0f);
		ImGui::Text("Dist to player : %.1f  (range %.0f)", dbgDist, detectRange);
		ImGui::TextColored(dbgInRange ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In range  : %s", dbgInRange ? "YES" : "no");
		ImGui::TextColored(dbgInFov ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In FOV    : %s", dbgInFov ? "YES" : "no");
		ImGui::TextColored(dbgLosClear ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "LOS clear : %s", dbgLosClear ? "YES" : "no");
		ImGui::TextColored(dbgCanSee ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "CAN SEE   : %s", dbgCanSee ? "YES" : "no");
		ImGui::Text("Search timer : %.1f", searchTimer);
	}

	if (ImGui::CollapsingHeader("Navigation"))
	{
		ImGui::Text("Grid built : %s", (navGrid && navGrid->IsBuilt()) ? "YES" : "no");
		ImGui::Text("Path nodes : %d", (int)path.size());
		ImGui::SliderFloat("Repath Interval", &repathInterval, 0.1f, 1.0f);
		ImGui::SliderFloat("Collision Radius", &collisionRadius, 0.1f, 1.0f);
		ImGui::SliderFloat("Collision Center Y", &collisionCenterY, 0.2f, 2.0f);
		ImGui::SliderFloat("Waypoint Radius", &waypointRadius, 0.15f, 1.0f);
		ImGui::Separator();
		ImGui::Checkbox("Ground Snap", &groundSnap);
		ImGui::SliderFloat("Ground Probe Up", &groundProbeUp, 0.2f, 3.0f);
		ImGui::SliderFloat("Ground Probe Down", &groundProbeDown, 0.5f, 8.0f);
		ImGui::Checkbox("Avoid Obstacles", &avoidEnable);
		ImGui::SliderFloat("Avoid Probe Dist", &avoidProbeDist, 0.5f, 5.0f);
		ImGui::SliderFloat("Avoid Align Weight", &avoidAlignWeight, 0.0f, 4.0f);
		ImGui::SliderFloat("Steer Smooth", &steerSmooth, 1.0f, 20.0f);
		ImGui::TextDisabled("NavMesh build/debug is in the 'NavMesh' window");
	}

	if (ImGui::CollapsingHeader("Attack"))
	{
		ImGui::SliderFloat("Attack Range", &attackRange, 0.5f, 6.0f);
		ImGui::SliderFloat("Attack Interval", &attackInterval, 0.2f, 5.0f);
		ImGui::SliderFloat("Hit Timing", &attackHitTiming, 0.0f, 1.0f);
		ImGui::SliderFloat("Attack Damage", &attackDamage, 0.0f, 50.0f);
	}

	if (ImGui::CollapsingHeader("Hit volume (sphere)"))
	{
		ImGui::SliderFloat("Center Y", &sphereCenterY, 0.0f, 3.0f);
		ImGui::SliderFloat("Radius", &sphereRadius, 0.2f, 3.0f);
		if (ImGui::Button("Damage 34")) ApplyDamage(34.0f);
	}

	ImGui::Separator();
	if (ImGui::Button("Save Transform")) SaveSettings();
	ImGui::SameLine();
	if (ImGui::Button("Load Transform")) LoadSettings();
}

// ---- Save / load transform ----
void EnemyComponent::SaveSettings()
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;
	std::ofstream ofs("EnemySettings.txt");
	if (!ofs) return;

	const DirectX::XMFLOAT3& p = owner->GetPosition();
	const DirectX::XMFLOAT4& r = owner->GetRotation();
	const DirectX::XMFLOAT3& s = owner->GetScale();
	ofs << p.x << " " << p.y << " " << p.z << "\n";
	ofs << r.x << " " << r.y << " " << r.z << " " << r.w << "\n";
	ofs << s.x << " " << s.y << " " << s.z << "\n";
}

void EnemyComponent::LoadSettings()
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;
	std::ifstream ifs("EnemySettings.txt");
	if (!ifs) return;

	DirectX::XMFLOAT3 p, s;
	DirectX::XMFLOAT4 r;
	ifs >> p.x >> p.y >> p.z;
	ifs >> r.x >> r.y >> r.z >> r.w;
	ifs >> s.x >> s.y >> s.z;
	owner->SetPosition(p);
	owner->SetRotation(r);
	owner->SetScale(s);
	currentYaw = QuatToYaw(r);
}
