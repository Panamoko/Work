#pragma once

#include <memory>
#include <vector>
#include <set>
#include <string>
#include <DirectXMath.h>

// 前方宣言
class Component;
struct RenderContext; // ActorManagerのRenderで使用

// アクター（コンポーネント指向におけるゲームオブジェクトの基底）
class Actor : public std::enable_shared_from_this<Actor>
{
public:
	Actor() {}
	virtual ~Actor() {};

	// 開始処理
	virtual void Start();

	// 更新処理
	virtual void Update(float elapsedTime);

	// 行列の更新
	virtual void UpdateTransform();

	// 描画登録処理（全コンポーネントのDrawを呼ぶ）
	virtual void Draw();

	// GUI表示
	virtual void OnGUI();

	// 名前の設定
	void SetName(const char* name) { this->name = name; }

	// 名前の取得
	const char* GetName() const { return name.c_str(); }

	// 位置の設定
	void SetPosition(const DirectX::XMFLOAT3& position) { this->position = position; }

	// 位置の取得
	const DirectX::XMFLOAT3& GetPosition() const { return position; }

	// 回転の設定
	void SetRotation(const DirectX::XMFLOAT4& rotation) { this->rotation = rotation; }

	// 回転の取得
	const DirectX::XMFLOAT4& GetRotation() const { return rotation; }

	// スケールの設定
	void SetScale(const DirectX::XMFLOAT3& scale) { this->scale = scale; }

	// スケールの取得
	const DirectX::XMFLOAT3& GetScale() const { return scale; }

	// 行列の取得
	const DirectX::XMFLOAT4X4& GetTransform() const { return transform; }

	// ========================================================
	// 階層（ヒエラルキー）管理用の機能
	// ========================================================
	void AddChild(std::shared_ptr<Actor> child)
	{
		child->parent = shared_from_this();
		children.push_back(child);
	}

	std::weak_ptr<Actor> GetParent() const { return parent; }
	const std::vector<std::shared_ptr<Actor>>& GetChildren() const { return children; }

	// ========================================================
	// コンポーネント管理機能
	// ========================================================
	// コンポーネント追加
	template<class T, class... Args>
	std::shared_ptr<T> AddComponent(Args... args)
	{
		std::shared_ptr<T> component = std::make_shared<T>(args...);
		component->SetActor(shared_from_this());
		components.emplace_back(component);
		return component;
	}

	// コンポーネント取得
	template<class T>
	std::shared_ptr<T> GetComponent()
	{
		for (std::shared_ptr<Component>& component : components)
		{
			std::shared_ptr<T> p = std::dynamic_pointer_cast<T>(component);
			if (p == nullptr) continue;
			return p;
		}
		return nullptr;
	}

private:
	std::string			name;
	DirectX::XMFLOAT3	position = DirectX::XMFLOAT3(0, 0, 0);
	DirectX::XMFLOAT4	rotation = DirectX::XMFLOAT4(0, 0, 0, 1);
	DirectX::XMFLOAT3	scale = DirectX::XMFLOAT3(1, 1, 1);
	DirectX::XMFLOAT4X4	transform = DirectX::XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

	// 親子関係用変数
	std::weak_ptr<Actor>					parent;
	std::vector<std::shared_ptr<Actor>>		children;

	// アタッチされたコンポーネントのリスト
	std::vector<std::shared_ptr<Component>>	components;
};


// アクターマネージャー
class ActorManager
{
private:
	ActorManager() {}
	~ActorManager() {};

public:
	// インスタンス取得
	static ActorManager& Instance()
	{
		static ActorManager instance;
		return instance;
	}

	// 作成
	std::shared_ptr<Actor> Create();

	// 削除
	void Remove(std::shared_ptr<Actor> actor);

	// 更新
	void Update(float elapsedTime);

	// 行列更新
	void UpdateTransform();

	// 描画（遅延描画キューイング方式に変更）
	void Render(const RenderContext& rc);

private:
	void DrawLister();
	void DrawDetail();

private:
	std::vector<std::shared_ptr<Actor>>		startActors;
	std::vector<std::shared_ptr<Actor>>		updateActors;
	std::set<std::shared_ptr<Actor>>		selectionActors;
	std::set<std::shared_ptr<Actor>>		removeActors;

	bool					hiddenLister = false;
	bool					hiddenDetail = false;
};