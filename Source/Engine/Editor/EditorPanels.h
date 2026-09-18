#pragma once
#include "Engine/Scene/EntityId.h"

namespace uel {
	class World;
	class CommandHistory;
	struct TransformEditSession;

	void DrawHierarchyPanel(World& world, EntityId& selected);
	void DrawInspectorPanel(World& world, EntityId selected, CommandHistory& history, TransformEditSession& session);
	void HandleShortcuts(CommandHistory& history);
	void DrawMainMenuBar(World& world, EntityId& selected, CommandHistory& history);
	void DrawHierarchyPanel(World& world, EntityId& selected, CommandHistory& history);
	void DrawAssetBrowserPanel(World& world, EntityId& selected, CommandHistory& history);
}