#include <imgui.h>
#include <functional> // std::function用（ツリー描画の再帰に使用）
#include "Graphics/Graphics.h"
#include "GameObject/Component.h"
#include "GameObject/Actor.h"
#include "Graphics/Model/ModelRenderer.h" // 遅延描画用にインクルード追加

// =====================================================================
// Actor クラスの実装
// =====================================================================

// 開始処理
void Actor::Start()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Start();
	}
}

// 更新処理
void Actor::Update(float elapsedTime)
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Update(elapsedTime);
	}
}

// 行列の更新（シーングラフ対応）
void Actor::UpdateTransform()
{
	DirectX::XMVECTOR Q = DirectX::XMLoadFloat4(&rotation);
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(Q);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

	DirectX::XMMATRIX W = S * R * T;

	std::shared_ptr<Actor> parentActor = parent.lock();
	if (parentActor != nullptr)
	{
		DirectX::XMMATRIX ParentW = DirectX::XMLoadFloat4x4(&parentActor->GetTransform());
		W = W * ParentW;
	}

	DirectX::XMStoreFloat4x4(&transform, W);
}

// 描画登録処理
void Actor::Draw()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Draw();
	}
}

// GUI表示（インスペクタ）
void Actor::OnGUI()
{
	// 名前
	{
		char buffer[1024];
		::strncpy_s(buffer, sizeof(buffer), GetName(), sizeof(buffer));
		if (ImGui::InputText("Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			SetName(buffer);
		}
	}

	// トランスフォーム
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ★プロのハック: DragFloat を使用。
		// 数字の部分を左右にドラッグするとスライダーのように滑らかに動き、上限/下限の縛りがありません。
		// 第3引数(0.1f)は、ドラッグした時の変化の速度です。
		ImGui::DragFloat3("Position", &position.x, 0.1f);
		ImGui::DragFloat4("Rotation", &rotation.x, 0.01f);
		ImGui::DragFloat3("Scale", &scale.x, 0.05f);
	}

	for (std::shared_ptr<Component>& component : components)
	{
		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::CollapsingHeader(component->GetName(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(component.get());
			component->OnGUI();
			ImGui::PopID();
		}
	}
}

// =====================================================================
// ActorManager クラスの実装
// =====================================================================

// 作成
std::shared_ptr<Actor> ActorManager::Create()
{
	std::shared_ptr<Actor> actor = std::make_shared<Actor>();
	{
		static int id = 0;
		char name[256];
		::sprintf_s(name, sizeof(name), "Actor%d", id++);
		actor->SetName(name);
	}
	startActors.emplace_back(actor);
	return actor;
}

// 削除
void ActorManager::Remove(std::shared_ptr<Actor> actor)
{
	removeActors.insert(actor);
}

// 更新
void ActorManager::Update(float elapsedTime)
{
	for (std::shared_ptr<Actor>& actor : startActors)
	{
		actor->Start();
		updateActors.emplace_back(actor);
	}
	startActors.clear();

	for (std::shared_ptr<Actor>& actor : updateActors)
	{
		actor->Update(elapsedTime);
	}

	for (const std::shared_ptr<Actor>& actor : removeActors)
	{
		std::vector<std::shared_ptr<Actor>>::iterator itStart = std::find(startActors.begin(), startActors.end(), actor);
		if (itStart != startActors.end())
		{
			startActors.erase(itStart);
		}

		std::vector<std::shared_ptr<Actor>>::iterator itUpdate = std::find(updateActors.begin(), updateActors.end(), actor);
		if (itUpdate != updateActors.end())
		{
			updateActors.erase(itUpdate);
		}

		std::set<std::shared_ptr<Actor>>::iterator itSelection = selectionActors.find(actor);
		if (itSelection != selectionActors.end())
		{
			selectionActors.erase(itSelection);
		}
	}
	removeActors.clear();
}

// 行列更新
void ActorManager::UpdateTransform()
{
	for (std::shared_ptr<Actor>& actor : updateActors)
	{
		actor->UpdateTransform();
	}
}

// 描画
void ActorManager::Render(const RenderContext& rc)
{
	for (std::shared_ptr<Actor>& actor : updateActors)
	{
		actor->Draw();
	}

	ModelRenderer* modelRenderer = Graphics::Instance().GetModelRenderer();
	if (modelRenderer != nullptr)
	{
		modelRenderer->Render(rc);
	}

	DrawLister();
	DrawDetail();
}

// リスター描画
void ActorManager::DrawLister()
{
	ImGui::SetNextWindowPos(ImVec2(30, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

	hiddenLister = !ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_None);
	if (!hiddenLister)
	{
		std::function<void(std::shared_ptr<Actor>)> drawNode = [&](std::shared_ptr<Actor> actor)
			{
				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;

				if (actor->GetChildren().empty())
				{
					nodeFlags |= ImGuiTreeNodeFlags_Leaf;
				}

				if (selectionActors.find(actor) != selectionActors.end())
				{
					nodeFlags |= ImGuiTreeNodeFlags_Selected;
				}

				bool isOpen = ImGui::TreeNodeEx(actor.get(), nodeFlags, "%s", actor->GetName());

				if (ImGui::IsItemClicked())
				{
					selectionActors.clear();
					selectionActors.insert(actor);
				}

				if (isOpen)
				{
					for (const std::shared_ptr<Actor>& child : actor->GetChildren())
					{
						drawNode(child);
					}
					ImGui::TreePop();
				}
			};

		for (std::shared_ptr<Actor>& actor : updateActors)
		{
			if (actor->GetParent().expired())
			{
				drawNode(actor);
			}
		}
	}
	ImGui::End();
}

// 詳細描画
void ActorManager::DrawDetail()
{
	ImGui::SetNextWindowPos(ImVec2(950, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

	hiddenDetail = !ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_None);
	if (!hiddenDetail)
	{
		std::shared_ptr<Actor> lastSelected = selectionActors.empty() ? nullptr : *selectionActors.rbegin();
		if (lastSelected != nullptr)
		{
			lastSelected->OnGUI();
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No Object Selected");
		}
	}
	ImGui::End();
}