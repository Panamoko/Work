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

	// 1秒あたりのフレーム数（FPS を固定する値）。0 にすると制限なし（重さを測るとき用）
	inline constexpr int kTargetFps = 60;

	// 1フレームの時間（dt）の上限（秒）。これより長く止まったフレームの後でも、ゲームとアニメはこの時間ぶんしか進めない
	// （ウィンドウのドラッグ中や、読み込みで止まった直後に、キャラが一気に動くのを防ぐ）
	inline constexpr float kMaxDeltaTime = 0.1f;

	// ゲームの更新に渡す dt にする：0 以下なら 1/60 秒、kMaxDeltaTime より長ければ kMaxDeltaTime
	inline float ClampDeltaTime(float dt) { return (dt <= 0.0f) ? (1.0f / 60.0f) : ((dt > kMaxDeltaTime) ? kMaxDeltaTime : dt); }
} // namespace uel
