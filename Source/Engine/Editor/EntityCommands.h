#pragma once
#include "Engine/Editor/CommandHistory.h"
#include "Engine/Scene/EntityId.h"
#include <string>

namespace uel {
	class World;

	// 生成 / 複製 / 削除（Undo/Redo対応）。Hierarchyから呼ぶ薄いAPI。
	EntityId EditorCreateEntity(World& world, CommandHistory& history, const char* name);
	EntityId EditorDuplicateEntity(World& world, CommandHistory& history, EntityId src);
	void     EditorDeleteEntity(World& world, CommandHistory& history, EntityId id);
	void     EditorRenameEntity(World& world, CommandHistory& history, EntityId id,
	const std::string& oldName, const std::string& newName);
	EntityId EditorSpawnPrefab(World& world, CommandHistory& history, const std::string& path);
} // namespace uel