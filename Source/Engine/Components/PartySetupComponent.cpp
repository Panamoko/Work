#include "Engine/Components/PartySetupComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/GameFlow.h"
#include "Engine/Game/Role.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/AllyComponent.h"
#include "Engine/Components/TankComponent.h"
#include "Engine/Components/DPSComponent.h"
#include "Engine/Components/SupportComponent.h"
#include "Engine/Serialization/StageSerializer.h"
#include <cmath>
#include <string>
#define NOMINMAX
#include <windows.h>    // OutputDebugStringA（VS の「出力」に理由を出す）

namespace uel {
    namespace {

        // その Entity のロールを付け替える（前のロールの Component は外す）
        void SetRole(Entity& e, Role role)
        {
            while (e.GetComponent<RoleComponent>()) e.RemoveComponent<RoleComponent>();
            switch (role) {
            case Role::Tank:    e.AddComponent<TankComponent>();    break;
            case Role::DPS:     e.AddComponent<DPSComponent>();     break;
            case Role::Support: e.AddComponent<SupportComponent>(); break;
            }
        }

    } // namespace

    void PartySetupComponent::Update(GameContext& ctx)
    {
        if (done) return;
        done = true;    // 成功しても失敗しても、1回だけ（毎フレーム Prefab を読みに行かない）

        // 1) プレイヤーに、選んだロールを付ける
        Entity* player = nullptr;
        for (const auto& slot : ctx.world.Slots())
            if (slot && slot->GetComponent<PlayerComponent>()) { player = slot.get(); break; }
        if (!player) return;    // プレイヤーのいない Scene（Title など）では何もしない
        auto* pt = player->GetComponent<TransformComponent>();
        if (!pt) return;

        const Role playerRole = ctx.flow ? ctx.flow->playerRole : kDefaultRole;
        SetRole(*player, playerRole);

        // 2) 残りの2つのロールの AI 仲間を、プレイヤーの左後ろ・右後ろに出す
        const float yaw = pt->rotationEuler.y * 0.01745329f;    // 度 → ラジアン
        const float fx = std::sin(yaw), fz = std::cos(yaw);     // プレイヤーの前
        const float rx = fz, rz = -fx;                          // プレイヤーの右（左手系・Y が上）
        float side = -1.0f;                                     // 1人目は左、2人目は右
        for (int r = 0; r < kRoleCount; ++r) {
            const Role role = (Role)r;
            if (role == playerRole) continue;

            EntityId id;
            if (allyPrefab.empty() || !StageSerializer::InstantiatePrefab(ctx.world, allyPrefab.c_str(), id)) {
                failed = true;
                const std::string msg = "[PartySetup] ally prefab not found: " + allyPrefab + "\n";
                OutputDebugStringA(msg.c_str());
                return;
            }
            Entity* ally = ctx.world.Get(id);
            if (!ally) continue;
            ally->name = std::string("Ally (") + RoleName(role) + ")";
            if (!ally->GetComponent<AllyComponent>()) ally->AddComponent<AllyComponent>();   // Prefab に付け忘れていても動くように
            SetRole(*ally, role);
            if (auto* at = ally->GetComponent<TransformComponent>()) {
                at->position = { pt->position.x - fx * spawnBack + rx * spawnSide * side,
                                 pt->position.y,
                                 pt->position.z - fz * spawnBack + rz * spawnSide * side };
                at->rotationEuler.y = pt->rotationEuler.y;      // プレイヤーと同じ向き
            }
            side = -side;
        }
    }

} // namespace uel
