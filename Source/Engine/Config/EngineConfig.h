#pragma once
#include <string>

namespace uel {

	// アセット（モデル等）のルートディレクトリ。
	// 置き場所を変えたいときはここだけ変更する。
	inline constexpr const char* kAssetRoot = "Data";

	// Scene の保存先フォルダ（Scene名.json・Scenes.json・Environment.json を置く）。
	// 置き場所を変えたいときはここだけ変更する。
	inline constexpr const char* kSceneDir = "Json";

	// kSceneDir の中のファイルのパスを作る（例：SceneFilePath("Stage.json") → "Json/Stage.json"）
	inline std::string SceneFilePath(const std::string& fileName) { return std::string(kSceneDir) + "/" + fileName; }

} // namespace uel
