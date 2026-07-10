#pragma once

#include <memory>
#include "GameObject/Actor.h"

// Base class for all components.
class Component
{
public:
	Component() {}
	virtual ~Component() {}

	virtual const char* GetName() const = 0;

	virtual void Start() {}
	virtual void Update(float elapsedTime) {}
	virtual void Draw() {}
	virtual void OnGUI() {}

	// Editor-only: refresh render state (e.g. push the owner transform to the
	// model) without advancing gameplay/animation. Called every Edit-mode frame.
	virtual void OnEditorSync() {}

	void SetActor(std::shared_ptr<Actor> actor) { this->actor = actor; }
	std::shared_ptr<Actor> GetActor() { return actor.lock(); }

private:
	std::weak_ptr<Actor>	actor;
};