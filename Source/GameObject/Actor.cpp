#include <imgui.h>
#include <functional> // std::function�p�i�c���[�`��̍ċA�Ɏg�p�j
#include <cmath>      // asinf / atan2f / copysignf
#include <cstring>    // strncpy_s / strstr

// ---------------------------------------------------------------------
// Inspector helpers (local to this translation unit)
// ---------------------------------------------------------------------
namespace
{
	// Quaternion -> Euler(degrees). For display (inverse of RollPitchYaw).
	DirectX::XMFLOAT3 QuatToEulerDeg(const DirectX::XMFLOAT4& q)
	{
		const float x = q.x, y = q.y, z = q.z, w = q.w;
		float sinp = 2.0f * (w * x - y * z);
		float pitch = (fabsf(sinp) >= 1.0f) ? copysignf(DirectX::XM_PIDIV2, sinp) : asinf(sinp);
		float yaw   = atan2f(2.0f * (w * y + x * z), 1.0f - 2.0f * (x * x + y * y));
		float roll  = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (x * x + z * z));
		return { DirectX::XMConvertToDegrees(pitch),
				 DirectX::XMConvertToDegrees(yaw),
				 DirectX::XMConvertToDegrees(roll) };
	}

	// Euler(degrees) -> Quaternion.
	DirectX::XMFLOAT4 EulerDegToQuat(const DirectX::XMFLOAT3& e)
	{
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(
			DirectX::XMConvertToRadians(e.x),
			DirectX::XMConvertToRadians(e.y),
			DirectX::XMConvertToRadians(e.z));
		DirectX::XMFLOAT4 r; DirectX::XMStoreFloat4(&r, q); return r;
	}

	// Small "Reset" button on the right edge. Returns true when pressed.
	bool ResetButton(const char* id)
	{
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.0f);
		ImGui::PushID(id);
		bool pressed = ImGui::SmallButton(u8"Reset");
		ImGui::PopID();
		return pressed;
	}
}
#include "Graphics/Graphics.h"
#include "GameObject/Component.h"
#include "GameObject/Actor.h"
#include "Graphics/Model/ModelRenderer.h" // �x���`��p�ɃC���N���[�h�ǉ�

// =====================================================================
// Actor �N���X�̎���
// =====================================================================

// �J�n����
void Actor::Start()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Start();
	}
}

// �X�V����
void Actor::Update(float elapsedTime)
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Update(elapsedTime);
	}
}

// �s��̍X�V�i�V�[���O���t�Ή��j
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

// �`��o�^����
void Actor::Draw()
{
	for (std::shared_ptr<Component>& component : components)
	{
		component->Draw();
	}
}

// GUI�\���i�C���X�y�N�^�j
void Actor::OnGUI()
{
	// ���O
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

	// �g�����X�t�H�[��
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ���v���̃n�b�N: DragFloat ���g�p�B
		// �����̕��������E�Ƀh���b�O����ƃX���C�_�[�̂悤�Ɋ��炩�ɓ����A���/�����̔��肪����܂���B
		// ��3����(0.1f)�́A�h���b�O�������̕ω��̑��x�ł��B
		// Position
		ImGui::DragFloat3(u8"Position", &position.x, 0.1f);
		if (ResetButton("rp")) position = DirectX::XMFLOAT3(0, 0, 0);

		// Rotation (edited as Euler degrees; stored internally as quaternion)
		DirectX::XMFLOAT3 euler = QuatToEulerDeg(rotation);
		if (ImGui::DragFloat3(u8"Rotation", &euler.x, 0.5f))
		{
			rotation = EulerDegToQuat(euler);
		}
		if (ResetButton("rr")) rotation = DirectX::XMFLOAT4(0, 0, 0, 1);

		// Scale
		ImGui::DragFloat3(u8"Scale", &scale.x, 0.05f, 0.001f, 1000.0f);
		if (ResetButton("rs")) scale = DirectX::XMFLOAT3(1, 1, 1);
	}

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
// ActorManager �N���X�̎���
// =====================================================================

// �쐬
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

// �폜
void ActorManager::Remove(std::shared_ptr<Actor> actor)
{
	removeActors.insert(actor);
}

// �X�V
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

// �s��X�V
void ActorManager::UpdateTransform()
{
	for (std::shared_ptr<Actor>& actor : updateActors)
	{
		actor->UpdateTransform();
	}
}

// �`��
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

// ���X�^�[�`��
void ActorManager::DrawLister()
{
	ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);

	hiddenLister = !ImGui::Begin(u8"Hierarchy");
	if (!hiddenLister)
	{
		// --- Toolbar: create + search ---
		if (ImGui::Button(u8"+ Create"))
		{
			std::shared_ptr<Actor> a = Create();
			selectionActors.clear();
			selectionActors.insert(a);
		}
		ImGui::SameLine();
		static char filter[128] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint(u8"##filter", u8"Search...", filter, sizeof(filter));
		ImGui::PopItemWidth();
		ImGui::Separator();

		const bool filtering = (filter[0] != '\0');

		std::function<void(std::shared_ptr<Actor>)> drawNode = [&](std::shared_ptr<Actor> actor)
			{
				ImGuiTreeNodeFlags nodeFlags =
					ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
					ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

				if (actor->GetChildren().empty())
				{
					nodeFlags |= ImGuiTreeNodeFlags_Leaf;
				}

				if (selectionActors.find(actor) != selectionActors.end())
				{
					nodeFlags |= ImGuiTreeNodeFlags_Selected;
				}

				bool isOpen = ImGui::TreeNodeEx(actor.get(), nodeFlags, "%s", actor->GetName());

				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					selectionActors.clear();
					selectionActors.insert(actor);
				}

				// Right-click context menu (selects this row first)
				if (ImGui::BeginPopupContextItem())
				{
					selectionActors.clear();
					selectionActors.insert(actor);
					if (ImGui::MenuItem(u8"Add Child"))
					{
						std::shared_ptr<Actor> child = Create();
						actor->AddChild(child);
					}
					if (ImGui::MenuItem(u8"Delete"))
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
			// While searching, show flat name matches (ignore hierarchy)
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
				{
					drawNode(actor);
				}
			}
		}

		// Click empty space to deselect
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			selectionActors.clear();
		}
	}
	ImGui::End();
}

// �ڍו`��
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
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), u8"No Object Selected");
			ImGui::TextDisabled(u8"Select an object in the Hierarchy");
		}
	}
	ImGui::End();
}