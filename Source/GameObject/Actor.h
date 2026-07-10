#pragma once

#include <memory>
#include <vector>
#include <set>
#include <string>
#include <DirectXMath.h>

class Component;
struct RenderContext;
struct CollisionMesh;

// Actor: the base game object in a component-based design.
class Actor : public std::enable_shared_from_this<Actor>
{
public:
	Actor() {}
	virtual ~Actor() {};

	virtual void Start();
	virtual void Update(float elapsedTime);
	virtual void UpdateTransform();
	virtual void Draw();
	virtual void OnGUI();

	// Editor-only: refresh render state of all components (no gameplay tick).
	void EditorSync();
	void SetName(const char* name) { this->name = name; }
	const char* GetName() const { return name.c_str(); }

	void SetPosition(const DirectX::XMFLOAT3& position) { this->position = position; }
	const DirectX::XMFLOAT3& GetPosition() const { return position; }

	void SetRotation(const DirectX::XMFLOAT4& rotation) { this->rotation = rotation; }
	const DirectX::XMFLOAT4& GetRotation() const { return rotation; }

	void SetScale(const DirectX::XMFLOAT3& scale) { this->scale = scale; }
	const DirectX::XMFLOAT3& GetScale() const { return scale; }

	const DirectX::XMFLOAT4X4& GetTransform() const { return transform; }

	// Set the collision mesh used by the Inspector "Drop to Ground" button.
	static void SetEditorGround(const CollisionMesh* mesh);

	// ========================================================
	// Hierarchy
	// ========================================================
	void AddChild(std::shared_ptr<Actor> child)
	{
		child->parent = shared_from_this();
		children.push_back(child);
	}

	std::weak_ptr<Actor> GetParent() const { return parent; }
	const std::vector<std::shared_ptr<Actor>>& GetChildren() const { return children; }

	// ========================================================
	// Components
	// ========================================================
	template<class T, class... Args>
	std::shared_ptr<T> AddComponent(Args... args)
	{
		std::shared_ptr<T> component = std::make_shared<T>(args...);
		component->SetActor(shared_from_this());
		components.emplace_back(component);
		return component;
	}

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

	std::weak_ptr<Actor>					parent;
	std::vector<std::shared_ptr<Actor>>		children;

	std::vector<std::shared_ptr<Component>>	components;
};


class ActorManager
{
private:
	ActorManager() {}
	~ActorManager() {};

public:
	static ActorManager& Instance()
	{
		static ActorManager instance;
		return instance;
	}

	std::shared_ptr<Actor> Create();
	void Remove(std::shared_ptr<Actor> actor);
	void Update(float elapsedTime);

	// Edit mode: process spawn/remove queues (runs Start) WITHOUT ticking gameplay.
	void SyncPending();

	void UpdateTransform();
	void Render(const RenderContext& rc);
	// Edit mode: refresh all actors' render transforms without ticking gameplay.
	void EditorSync();
	std::shared_ptr<Actor> GetSelectedActor()
	{
		if (selectionActors.empty()) return nullptr;
		return *selectionActors.rbegin();
	}
	// All live actors (for viewport picking).
	const std::vector<std::shared_ptr<Actor>>& GetActors() const { return updateActors; }

	// Set the current selection (nullptr clears it).
	void SetSelected(std::shared_ptr<Actor> a)
	{
		selectionActors.clear();
		if (a) selectionActors.insert(a);
	}
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