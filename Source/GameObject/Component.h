#pragma once

#include <memory>
#include "GameObject/Actor.h" // フォルダ構成に合わせてパスを修正（エラーが出る場合は "Actor.h" に戻してください）

// コンポーネントの基底クラス
class Component
{
public:
	Component() {}
	virtual ~Component() {}

	// 名前取得
	virtual const char* GetName() const = 0;

	// 開始処理
	virtual void Start() {}

	// 更新処理
	virtual void Update(float elapsedTime) {}

	
	// ここにあった古い Render(ID3D11DeviceContext* dc, ...) を削除しました。
	virtual void Draw() {}

	// GUI描画
	virtual void OnGUI() {}

	// アクター設定
	void SetActor(std::shared_ptr<Actor> actor) { this->actor = actor; }

	// アクター取得
	std::shared_ptr<Actor> GetActor() { return actor.lock(); }

private:
	std::weak_ptr<Actor>	actor;
};