#include <imgui.h>
#include <functional>
#include <cmath>
#include <cstring>
#include "Graphics/Graphics.h"
#include "GameObject/Component.h"
#include "GameObject/Actor.h"
#include "Graphics/Model/ModelRenderer.h"

// =====================================================================
// Actor クラスの実装
// =====================================================================

void Actor::Start()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Start();
	}
}

void Actor::Update(float elapsedTime)
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Update(elapsedTime);
	}
}

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

void Actor::Draw()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Draw();
	}
}

// ---------------------------------------------------------------------
// Inspector 補助関数（このファイル内だけで使う）
// ---------------------------------------------------------------------
namespace
{
	// クォータニオン → オイラー角（度）
	DirectX::XMFLOAT3 QuatToEulerDeg(const DirectX::XMFLOAT4& q)
	{
		const float x = q.x, y = q.y, z = q.z, w = q.w;
		float sinp = 2.0f * (w * x - y * z);
		float pitch = (fabsf(sinp) >= 1.0f) ? copysignf(DirectX::XM_PIDIV2, sinp) : asinf(sinp);
		float yaw = atan2f(2.0f * (w * y + x * z), 1.0f - 2.0f * (x * x + y * y));
		float roll = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (x * x + z * z));
		return { DirectX::XMConvertToDegrees(pitch),
				 DirectX::XMConvertToDegrees(yaw),
				 DirectX::XMConvertToDegrees(roll) };
	}

	// オイラー角（度）→ クォータニオン
	DirectX::XMFLOAT4 EulerDegToQuat(const DirectX::XMFLOAT3& e)
	{
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(
			DirectX::XMConvertToRadians(e.x),
			DirectX::XMConvertToRadians(e.y),
			DirectX::XMConvertToRadians(e.z));
		DirectX::XMFLOAT4 r;
		DirectX::XMStoreFloat4(&r, q);
		return r;
	}

	// 行の右端に小さな Reset ボタン。押されたら true
	bool ResetButton(const char* id)
	{
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.0f);
		ImGui::PushID(id);
		bool pressed = ImGui::SmallButton(u8"Reset");
		ImGui::PopID();
		return pressed;
	}
}

// GUI 表示（Inspector）
void Actor::OnGUI()
{
	// 名前（フル幅）
	{
		char buffer[256];
		::strncpy_s(buffer, sizeof(buffer), GetName(), _TRUNCATE);
		ImGui::PushItemWidth(-1);
		if (ImGui::InputText(u8"##Name", buffer, sizeof(buffer)))
		{
			SetName(buffer);
		}
		ImGui::PopItemWidth();
	}

	// Transform
	if (ImGui::CollapsingHeader(u8"Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3(u8"Position", &position.x, 0.1f);
		if (ResetButton("rp")) position = DirectX::XMFLOAT3(0, 0, 0);

		// 回転はオイラー角（度）で編集。内部はクォータニオンのまま
		DirectX::XMFLOAT3 euler = QuatToEulerDeg(rotation);
		if (ImGui::DragFloat3(u8"Rotation", &euler.x, 0.5f))
		{
			rotation = EulerDegToQuat(euler);
		}
		if (ResetButton("rr")) rotation = DirectX::XMFLOAT4(0, 0, 0, 1);

		ImGui::DragFloat3(u8"Scale", &scale.x, 0.05f, 0.001f, 1000.0f);
		if (ResetButton("rs")) scale = DirectX::XMFLOAT3(1, 1, 1);
	}

	// 各コンポーネント
	for (std::shared_ptr<Component>& component : components)
	{
		ImGui::Spacing();
		if (ImGui::CollapsingHeader(component->GetName(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(component.get());
			ImGui::Indent(8.0f);
			component->OnGUI();
			ImGui::Unindent(8.0f);
			ImGui::PopID();
		}
	}
}

// =====================================================================
// ActorManager クラスの実装
// =====================================================================

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

void ActorManager::Remove(std::shared_ptr<Actor> actor)
{
	removeActors.insert(actor);
}

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
		auto itStart = std::find(startActors.begin(), startActors.end(), actor);
		if (itStart != startActors.end()) startActors.erase(itStart);

		auto itUpdate = std::find(updateActors.begin(), updateActors.end(), actor);
		if (itUpdate != updateActors.end()) updateActors.erase(itUpdate);

		auto itSel = selectionActors.find(actor);
		if (itSel != selectionActors.end()) selectionActors.erase(itSel);
	}
	removeActors.clear();
}

void ActorManager::UpdateTransform()
{
	for (std::shared_ptr<Actor>& actor : updateActors)
	{
		actor->UpdateTransform();
	}
}

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

// Hierarchy ウィンドウ
void ActorManager::DrawLister()
{
	ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);

	hiddenLister = !ImGui::Begin(u8"Hierarchy");
	if (!hiddenLister)
	{
		// ツールバー：新規作成 ＋ 検索
		if (ImGui::Button(u8"+ 新規作成"))
		{
			std::shared_ptr<Actor> a = Create();
			selectionActors.clear();
			selectionActors.insert(a);
		}
		ImGui::SameLine();
		static char filter[128] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint(u8"##filter", u8"検索...", filter, sizeof(filter));
		ImGui::PopItemWidth();
		ImGui::Separator();

		const bool filtering = (filter[0] != '\0');

		std::function<void(std::shared_ptr<Actor>)> drawNode = [&](std::shared_ptr<Actor> actor)
			{
				ImGuiTreeNodeFlags nodeFlags =
					ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
					ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

				if (actor->GetChildren().empty())
					nodeFlags |= ImGuiTreeNodeFlags_Leaf;
				if (selectionActors.find(actor) != selectionActors.end())
					nodeFlags |= ImGuiTreeNodeFlags_Selected;

				bool isOpen = ImGui::TreeNodeEx(actor.get(), nodeFlags, "%s", actor->GetName());

				// 三角の開閉クリックでは選択しない
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					selectionActors.clear();
					selectionActors.insert(actor);
				}

				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem())
				{
					selectionActors.clear();
					selectionActors.insert(actor);
					if (ImGui::MenuItem(u8"子を追加"))
					{
						actor->AddChild(Create());
					}
					if (ImGui::MenuItem(u8"削除"))
					{
						Remove(actor);
					}
					ImGui::EndPopup();
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

		if (filtering)
		{
			for (std::shared_ptr<Actor>& actor : updateActors)
			{
				if (::strstr(actor->GetName(), filter) == nullptr) continue;
				bool selected = selectionActors.find(actor) != selectionActors.end();
				if (ImGui::Selectable(actor->GetName(), selected))
				{
					selectionActors.clear();
					selectionActors.insert(actor);
				}
			}
		}
		else
		{
			for (std::shared_ptr<Actor>& actor : updateActors)
			{
				if (actor->GetParent().expired())
					drawNode(actor);
			}
		}

		// 空白クリックで選択解除
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			selectionActors.clear();
		}
	}
	ImGui::End();
}

// Inspector ウィンドウ
void ActorManager::DrawDetail()
{
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 320.0f, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);

	hiddenDetail = !ImGui::Begin(u8"Inspector");
	if (!hiddenDetail)
	{
		std::shared_ptr<Actor> lastSelected = selectionActors.empty() ? nullptr : *selectionActors.rbegin();
		if (lastSelected != nullptr)
		{
			lastSelected->OnGUI();
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), u8"オブジェクト未選択");
			ImGui::TextDisabled(u8"Hierarchy でオブジェクトを選択してください");
		}
	}
	ImGui::End();
}