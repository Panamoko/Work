#pragma once
#include "Engine/Scene/Entity.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/AllyComponent.h"
#include "Engine/Components/OpponentComponent.h"
#include <cstdint>

namespace uel {

    // 陣営（UE の FGenericTeamId に当たる）。
    // 陣営は「誰が操作しているか」で決まる：プレイヤー（PlayerComponent）と AI 仲間（AllyComponent）は Players、
    // 敵（OpponentComponent）は Enemies、どれも持たない物（壁・的・箱など）は None。
    // UE でも、陣営は Pawn ではなく Controller が持つ（AAIController の IGenericTeamAgentInterface）。
    // 陣営を Entity ごとに自由に決めたくなったら、TeamComponent を足して GetTeam がそれを先に見るようにする
    // （呼んでいる側のコードは変わらない）。
    enum class Team : std::uint8_t { None, Players, Enemies };

    // 2つの Entity の関係（UE の ETeamAttitude に当たる）
    enum class TeamAttitude : std::uint8_t { Friendly, Neutral, Hostile };

    inline Team GetTeam(const Entity& e)
    {
        if (e.GetComponent<PlayerComponent>() || e.GetComponent<AllyComponent>()) return Team::Players;
        if (e.GetComponent<OpponentComponent>()) return Team::Enemies;
        return Team::None;
    }

    // 同じ陣営なら Friendly、違う陣営なら Hostile、どちらかが陣営なし（None）なら Neutral
    inline TeamAttitude GetAttitude(const Entity& a, const Entity& b)
    {
        const Team ta = GetTeam(a), tb = GetTeam(b);
        if (ta == Team::None || tb == Team::None) return TeamAttitude::Neutral;
        return (ta == tb) ? TeamAttitude::Friendly : TeamAttitude::Hostile;
    }

    inline bool IsFriendly(const Entity& a, const Entity& b) { return GetAttitude(a, b) == TeamAttitude::Friendly; }
    inline bool IsHostile(const Entity& a, const Entity& b) { return GetAttitude(a, b) == TeamAttitude::Hostile; }

} // namespace uel
