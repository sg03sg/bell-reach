#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Input.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Skills.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace Art;

//
// 가방(I)과 단축칸(1~9).
//
//   · 가방을 열면 세계가 멈춘다. 왼쪽은 가방 칸, 오른쪽은 파티다.
//   · 가방 칸의 물건을 화면 아래 단축칸으로 끌어다 놓으면 그 숫자 키로 쓴다.
//     단축칸끼리 끌면 자리를 바꾸고, 단축칸에서 밖으로 끌어내면 뺀다.
//   · 가방 칸에 마우스를 올리고 숫자 키를 눌러도 그 단축칸에 걸린다.
//   · 오른쪽 단추나 두 번 누르기로 바로 쓴다.
//   · 파티 칸의 "보내기"로 고용한 모험가를 돌려보낸다.
//
// 단축칸은 가방이 닫혀 있어도 늘 화면 아래에 보인다.
//
namespace
{
	// ── 가방 창 ──
	const float kPanelWidth = 740.0f;
	const float kPanelHeight = 372.0f;
	const float kPanelCenterY = 36.0f;
	const float kSlot = 52.0f;
	const float kSlotGap = 6.0f;
	const float kGridWidth = Inventory::kColumns * kSlot + (Inventory::kColumns - 1) * kSlotGap;
	const float kGridHeight = Inventory::kRows * kSlot + (Inventory::kRows - 1) * kSlotGap;
	const float kPanelLeft = -kPanelWidth * 0.5f;
	const float kPanelTop = kPanelCenterY + kPanelHeight * 0.5f;
	const float kGridLeft = kPanelLeft + 26.0f;
	const float kGridTop = kPanelTop - 58.0f;

	// ── 파티 칸 ──
	const float kPartyLeft = kGridLeft + kGridWidth + 34.0f;
	const float kPartyRight = kPanelLeft + kPanelWidth - 26.0f;
	const float kPartyTop = kGridTop;
	const float kPartyRow = 46.0f;
	const float kButtonWidth = 62.0f;
	const float kButtonHeight = 24.0f;

	// ── 단축칸 ──
	const float kHotSlot = 40.0f;
	const float kHotGap = 5.0f;

	const float kDoubleClick = 0.35f;

	const Color kPanel     = { 0.035f, 0.038f, 0.050f, 0.95f };
	const Color kRim       = { 0.42f, 0.36f, 0.24f, 0.6f };
	const Color kShade     = { 0.0f, 0.0f, 0.0f, 0.42f };
	const Color kSlotColor = { 0.075f, 0.080f, 0.100f, 1.0f };
	const Color kSlotRim   = { 0.22f, 0.22f, 0.26f, 1.0f };
	const Color kHover     = { 0.96f, 0.80f, 0.46f, 1.0f };
	const Color kTitle     = { 0.96f, 0.88f, 0.68f, 1.0f };
	const Color kLabel     = { 0.56f, 0.58f, 0.63f, 1.0f };
	const Color kValue     = { 0.93f, 0.93f, 0.95f, 1.0f };

	float BagSlotX(int index)
	{
		return kGridLeft + kSlot * 0.5f + static_cast<float>(index % Inventory::kColumns) * (kSlot + kSlotGap);
	}

	float BagSlotY(int index)
	{
		return kGridTop - kSlot * 0.5f - static_cast<float>(index / Inventory::kColumns) * (kSlot + kSlotGap);
	}

	float HotSlotX(int index)
	{
		const float width = Inventory::kHotbar * kHotSlot + (Inventory::kHotbar - 1) * kHotGap;
		return -width * 0.5f + kHotSlot * 0.5f + static_cast<float>(index) * (kHotSlot + kHotGap);
	}

	bool Inside(float x, float y, float centerX, float centerY, float halfWidth, float halfHeight)
	{
		return std::fabs(x - centerX) <= halfWidth && std::fabs(y - centerY) <= halfHeight;
	}
}

// ═══ 쓰기 ═════════════════════════════════════════════════════════

void Game::NoteItem(const std::string& text)
{
	m_ItemNote = text;
	m_ItemNoteTime = 2.4f;
}

bool Game::UseItem(ItemKind kind)
{
	if (kind == ItemKind::None)
	{
		return false;
	}
	const ItemInfo& info = ItemOf(kind);
	if (m_Inventory.Count(kind) <= 0)
	{
		NoteItem(std::string(info.name) + " — 가방에 없다");
		return false;
	}

	switch (kind)
	{
	case ItemKind::HealPack:
		if (Stats().health >= m_Player->MaxHealth() - 0.5f)
		{
			NoteItem("회복약 — 체력이 가득하다");
			return false;
		}
		m_Player->Heal(m_Player->MaxHealth() * 0.35f);
		SpawnFloatingText(m_PlayerX, m_PlayerY - 44.0f, "회복약", -1, kHealRed, 14.0f);
		break;

	case ItemKind::Torch:
		m_Scene.Spawn(std::unique_ptr<Actor>(new TorchActor(m_PlayerX, m_PlayerY + 6.0f)));
		break;

	case ItemKind::Oil:
		if (m_Player->Oil() >= 0.99f)
		{
			NoteItem("기름병 — 등불이 가득 차 있다");
			return false;
		}
		m_Player->Refuel(0.5f);
		SpawnFloatingText(m_PlayerX, m_PlayerY - 44.0f, "기름", -1, kFlame, 14.0f);
		break;

	case ItemKind::WeaponStone:
		if (Stats().weaponTier >= Progression::kMaxWeaponTier)
		{
			NoteItem("강화석 — 더 벼릴 데가 없다");
			return false;
		}
		m_Player->AddWeaponTier();
		SpawnFloatingText(m_PlayerX, m_PlayerY - 44.0f, "무기 강화", -1, kGold, 16.0f);
		break;

	default:
		return false;
	}

	m_Inventory.Remove(kind);
	NoteItem(std::string(info.name) + " — 남은 것 " + std::to_string(m_Inventory.Count(kind)));
	return true;
}

void Game::UseHotkeys()
{
	for (int number = 1; number <= Inventory::kHotbar; ++number)
	{
		if (!KeyPressed(static_cast<unsigned char>('0' + number)))
		{
			continue;
		}
		const ItemKind kind = m_Inventory.Hotkey(number - 1);
		if (kind == ItemKind::None)
		{
			NoteItem(std::to_string(number) + "번 단축칸이 비어 있다 — I로 가방을 열어 끌어다 놓는다");
			continue;
		}
		UseItem(kind);
	}
}

// ═══ 어디를 가리키는가 ════════════════════════════════════════════

int Game::BagSlotAt(float x, float y) const
{
	for (int i = 0; i < Inventory::kSlots; ++i)
	{
		if (Inside(x, y, BagSlotX(i), BagSlotY(i), kSlot * 0.5f, kSlot * 0.5f))
		{
			return i;
		}
	}
	return -1;
}

int Game::HotbarSlotAt(float x, float y) const
{
	const float centerY = -m_WindowHeight * 0.5f + 24.0f;
	for (int i = 0; i < Inventory::kHotbar; ++i)
	{
		if (Inside(x, y, HotSlotX(i), centerY, kHotSlot * 0.5f + kHotGap * 0.5f, kHotSlot * 0.5f + 4.0f))
		{
			return i;
		}
	}
	return -1;
}

int Game::PartyButtonAt(float x, float y) const
{
	// 첫 줄은 동행자 자리다(있든 없든 비워 둔다). 파티는 둘째 줄부터.
	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		const float rowY = kPartyTop - 40.0f - kPartyRow * static_cast<float>(i + 1);
		if (Inside(x, y, kPartyRight - kButtonWidth * 0.5f, rowY - 14.0f, kButtonWidth * 0.5f, kButtonHeight * 0.5f))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

// ═══ 가방이 열려 있는 동안 ════════════════════════════════════════

void Game::UpdateInventory()
{
	const int bag = BagSlotAt(m_MouseX, m_MouseY);
	const int hot = HotbarSlotAt(m_MouseX, m_MouseY);
	const ItemKind bagKind = (bag >= 0) ? m_Inventory.At(bag).kind : ItemKind::None;

	// 가리킨 가방 칸의 물건을 숫자 키의 단축칸에 건다
	for (int number = 1; number <= Inventory::kHotbar; ++number)
	{
		if (KeyPressed(static_cast<unsigned char>('0' + number)) && bagKind != ItemKind::None)
		{
			m_Inventory.Bind(number - 1, bagKind);
			NoteItem(std::string(ItemOf(bagKind).name) + " → " + std::to_string(number) + "번 단축칸");
		}
	}

	// ── 왼쪽 단추를 눌렀다 ──
	if (m_MousePressed[Input::kMouseLeft])
	{
		const int button = PartyButtonAt(m_MouseX, m_MouseY);
		if (button >= 0)
		{
			if (VillagerActor* member = FindVillager(m_Party[static_cast<size_t>(button)]))
			{
				NoteItem(member->DisplayName() + " — 돌려보냈다");
				EndContract(member, false);
			}
		}
		else if (bagKind != ItemKind::None)
		{
			if (m_LastClickSlot == bag && m_Time - m_LastClickTime < kDoubleClick)
			{
				UseItem(bagKind);   // 두 번 눌렀다
				m_LastClickSlot = -1;
			}
			else
			{
				m_Drag = DragState();
				m_Drag.active = true;
				m_Drag.index = bag;
				m_Drag.kind = bagKind;
				m_Drag.startX = m_MouseX;
				m_Drag.startY = m_MouseY;
				m_LastClickSlot = bag;
				m_LastClickTime = m_Time;
			}
		}
		else if (hot >= 0 && m_Inventory.Hotkey(hot) != ItemKind::None)
		{
			m_Drag = DragState();
			m_Drag.active = true;
			m_Drag.fromHotbar = true;
			m_Drag.index = hot;
			m_Drag.kind = m_Inventory.Hotkey(hot);
			m_Drag.startX = m_MouseX;
			m_Drag.startY = m_MouseY;
		}
	}

	// ── 끄는 중 ──
	if (m_Drag.active)
	{
		if (Distance(m_MouseX, m_MouseY, m_Drag.startX, m_Drag.startY) > 4.0f)
		{
			m_Drag.moved = true;
		}

		if (!m_MouseHeld[Input::kMouseLeft])
		{
			if (m_Drag.moved)
			{
				if (hot >= 0)
				{
					if (m_Drag.fromHotbar)
					{
						m_Inventory.SwapHotkeys(m_Drag.index, hot);
					}
					else
					{
						m_Inventory.Bind(hot, m_Drag.kind);
						NoteItem(std::string(ItemOf(m_Drag.kind).name) + " → " + std::to_string(hot + 1) + "번 단축칸");
					}
				}
				else if (bag >= 0 && !m_Drag.fromHotbar)
				{
					m_Inventory.Move(m_Drag.index, bag);
				}
				else if (m_Drag.fromHotbar)
				{
					m_Inventory.Unbind(m_Drag.index);   // 단축칸 밖으로 끌어냈다
				}
			}
			m_Drag = DragState();
		}
	}

	// ── 오른쪽 단추: 바로 쓴다 ──
	if (m_MousePressed[Input::kMouseRight])
	{
		if (bagKind != ItemKind::None)
		{
			UseItem(bagKind);
		}
		else if (hot >= 0)
		{
			UseItem(m_Inventory.Hotkey(hot));
		}
	}
}

// ═══ 그리기 ═══════════════════════════════════════════════════════

void Game::DrawHotbar(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Hud);

	const float centerY = -m_WindowHeight * 0.5f + 24.0f;
	const int hover = m_InventoryOpen ? HotbarSlotAt(m_MouseX, m_MouseY) : -1;
	const Color backing = { 0.015f, 0.017f, 0.025f, 0.82f };
	const Color rim = { kBronze[0], kBronze[1], kBronze[2], 0.45f };
	const Color number = { 0.62f, 0.62f, 0.66f, 0.9f };

	for (int i = 0; i < Inventory::kHotbar; ++i)
	{
		const float x = HotSlotX(i);
		const ItemKind kind = m_Inventory.Hotkey(i);
		const int count = m_Inventory.Count(kind);
		const bool lifted = m_Drag.active && m_Drag.moved && m_Drag.fromHotbar && m_Drag.index == i;

		renderer->DrawRoundRect(x, centerY, kHotSlot + 2.0f, kHotSlot + 2.0f, 6.0f, (i == hover) ? kHover : rim);
		renderer->DrawRoundRect(x, centerY, kHotSlot - 1.0f, kHotSlot - 1.0f, 5.0f, backing);

		if (kind != ItemKind::None && !lifted)
		{
			DrawItemIcon(renderer, kind, x, centerY, kHotSlot * 0.78f, count > 0 ? 1.0f : 0.28f);
			if (count > 0)
			{
				const std::string text = std::to_string(count);
				DrawDigits(renderer, text, 11.0f, x + kHotSlot * 0.5f - 3.0f - MeasureDigits(renderer, text, 11.0f),
				           centerY - kHotSlot * 0.5f + 13.0f, kValue);
			}
		}
		DrawDigits(renderer, std::to_string(i + 1), 10.0f, x - kHotSlot * 0.5f + 3.0f, centerY + kHotSlot * 0.5f - 1.0f,
		           number);
	}

	// 아무것도 걸지 않았으면 어떻게 거는지 한 줄로 알린다(가방에 무언가 있을 때만)
	if (!m_InventoryOpen && m_Inventory.HotbarEmpty())
	{
		bool anything = false;
		for (int i = 0; i < Inventory::kSlots && !anything; ++i)
		{
			anything = m_Inventory.At(i).kind != ItemKind::None;
		}
		if (anything)
		{
			const std::string hint = "I  가방을 열어 물건을 단축칸으로 끌어다 놓기";
			const float width = renderer->MeasureLabel(hint, 12.0f);
			renderer->DrawLabel(hint, 12.0f, -width * 0.5f, centerY + kHotSlot * 0.5f + 20.0f,
			                    Color{ 0.78f, 0.74f, 0.64f, 0.75f + 0.25f * std::sin(m_Time * 3.0f) });
		}
	}
}

void Game::DrawItemNote(Renderer* renderer)
{
	if (m_ItemNoteTime <= 0.0f || m_ItemNote.empty() || m_InventoryOpen)
	{
		return;
	}
	renderer->SetTag(DrawTag::Hud);
	const float fade = std::min(1.0f, m_ItemNoteTime / 0.6f);
	const float y = -m_WindowHeight * 0.5f + 24.0f + kHotSlot * 0.5f + 22.0f;
	const float width = renderer->MeasureLabel(m_ItemNote, 13.0f);
	renderer->DrawRoundRect(0.0f, y - 7.0f, width + 22.0f, 22.0f, 10.0f, Color{ 0.02f, 0.022f, 0.03f, 0.78f * fade });
	renderer->DrawLabel(m_ItemNote, 13.0f, -width * 0.5f, y + 2.0f, Color{ 0.92f, 0.88f, 0.78f, fade });
}

void Game::DrawInventory(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Hud);

	// ── 창 ──
	renderer->DrawRoundRect(0.0f, 0.0f, static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight),
	                        0.0f, kShade);
	renderer->DrawRoundRect(0.0f, kPanelCenterY, kPanelWidth + 3.0f, kPanelHeight + 3.0f, 12.0f, kRim);
	renderer->DrawRoundRect(0.0f, kPanelCenterY, kPanelWidth, kPanelHeight, 11.0f, kPanel);

	renderer->DrawLabel("가방", 18.0f, kGridLeft, kPanelTop - 20.0f, kTitle);
	{
		const std::string gold = std::to_string(Stats().gold);
		const float goldWidth = MeasureDigits(renderer, gold, 15.0f);
		const float right = kGridLeft + kGridWidth;
		renderer->DrawEllipse(right - goldWidth - 12.0f, kPanelTop - 28.0f, 6.0f, 6.0f, Tint(kGold), 0.0f, 0.6f);
		DrawDigits(renderer, gold, 15.0f, right - goldWidth, kPanelTop - 19.0f, Tint(kGold, 1.1f));
	}

	// ── 가방 칸 ──
	const int hover = BagSlotAt(m_MouseX, m_MouseY);
	for (int i = 0; i < Inventory::kSlots; ++i)
	{
		const float x = BagSlotX(i);
		const float y = BagSlotY(i);
		const Inventory::Slot& slot = m_Inventory.At(i);
		const bool lifted = m_Drag.active && m_Drag.moved && !m_Drag.fromHotbar && m_Drag.index == i;

		renderer->DrawRoundRect(x, y, kSlot + 2.0f, kSlot + 2.0f, 7.0f, (i == hover) ? kHover : kSlotRim);
		renderer->DrawRoundRect(x, y, kSlot - 1.0f, kSlot - 1.0f, 6.0f, kSlotColor);

		if (slot.kind != ItemKind::None && !lifted)
		{
			DrawItemIcon(renderer, slot.kind, x, y + 2.0f, kSlot * 0.72f);
			const std::string text = std::to_string(slot.count);
			DrawDigits(renderer, text, 12.0f, x + kSlot * 0.5f - 4.0f - MeasureDigits(renderer, text, 12.0f),
			           y - kSlot * 0.5f + 15.0f, kValue);

			// 이 물건이 걸린 단축칸 번호를 왼쪽 위에 작게
			for (int k = 0; k < Inventory::kHotbar; ++k)
			{
				if (m_Inventory.Hotkey(k) == slot.kind)
				{
					DrawDigits(renderer, std::to_string(k + 1), 10.0f, x - kSlot * 0.5f + 4.0f, y + kSlot * 0.5f - 2.0f,
					           kHover);
					break;
				}
			}
		}
	}

	// ── 가리킨 물건의 설명, 또는 방금 일어난 일 ──
	{
		const float noteTop = kGridTop - kGridHeight - 16.0f;
		const ItemKind kind = (hover >= 0) ? m_Inventory.At(hover).kind : ItemKind::None;
		if (kind != ItemKind::None)
		{
			const ItemInfo& info = ItemOf(kind);
			const float nameWidth = renderer->DrawLabel(info.name, 14.0f, kGridLeft, noteTop, kTitle);
			renderer->DrawLabel(info.note, 12.0f, kGridLeft + nameWidth + 8.0f, noteTop - 1.0f, kValue);
		}
		else if (m_ItemNoteTime > 0.0f && !m_ItemNote.empty())
		{
			renderer->DrawLabel(m_ItemNote, 13.0f, kGridLeft, noteTop, kValue);
		}
		renderer->DrawLabel("끌어서 단축칸에 놓기   숫자 키로 바로 걸기   우클릭 · 두 번 눌러 쓰기", 11.0f,
		                    kGridLeft, noteTop - 22.0f, kLabel);
		renderer->DrawLabel("I · ESC  닫기", 11.0f, kGridLeft, noteTop - 40.0f, kLabel);
	}

	// ── 파티 ──
	{
		float x = kPartyLeft;
		x += renderer->DrawLabel("파티", 18.0f, x, kPanelTop - 20.0f, kTitle) + 10.0f;
		DrawDigits(renderer, std::to_string(m_Party.size()) + "/" + std::to_string(kMaxParty), 14.0f,
		           x, kPanelTop - 23.0f, kLabel);

		// 첫 줄은 동행자. 파티에 세지 않고, 돌려보낼 수도 없다(종탑까지 함께 가야 한다).
		float rowY = kPartyTop - 40.0f;
		if (m_Companion != NULL)
		{
			const Skills::Info& skill = Skills::At(m_Companion->Profession());
			const Color tint = { skill.tint[0], skill.tint[1], skill.tint[2], 1.0f };
			renderer->DrawCircle(kPartyLeft + 6.0f, rowY - 8.0f, 5.0f, tint);
			const float width = renderer->DrawLabel(std::string("동행 · ") + skill.who, 14.0f, kPartyLeft + 18.0f, rowY, tint);
			renderer->DrawLabel(m_Companion->IsFrozenAgain() ? "굳어 있다 — 빛을 비춰 깨우기" : "종탑까지 함께 간다",
			                    11.0f, kPartyLeft + 18.0f + width + 10.0f, rowY - 2.0f, kLabel);
		}
		else
		{
			renderer->DrawLabel("동행 없음 — 굳은 사람을 깨우면 종탑까지 함께 간다", 11.0f, kPartyLeft, rowY - 2.0f, kLabel);
		}

		const int button = PartyButtonAt(m_MouseX, m_MouseY);
		for (int i = 0; i < kMaxParty; ++i)
		{
			rowY = kPartyTop - 40.0f - kPartyRow * static_cast<float>(i + 1);
			renderer->DrawRoundRect((kPartyLeft + kPartyRight) * 0.5f, rowY + kPartyRow * 0.5f - 6.0f,
			                        kPartyRight - kPartyLeft, 1.0f, 0.5f, Color{ 0.2f, 0.21f, 0.25f, 0.7f });

			const VillagerActor* member = (static_cast<size_t>(i) < m_Party.size())
				? FindVillager(m_Party[static_cast<size_t>(i)]) : NULL;
			if (member == NULL)
			{
				renderer->DrawLabel("빈자리 — 모험가에게 말을 걸어 고용한다", 11.0f, kPartyLeft, rowY - 6.0f,
				                    Color{ 0.38f, 0.40f, 0.45f, 1.0f });
				continue;
			}

			const CallingInfo& calling = CallingOf(member->GetCalling());
			const Color tint = { calling.tint[0], calling.tint[1], calling.tint[2], 1.0f };
			renderer->DrawRoundRect(kPartyLeft + 6.0f, rowY - 8.0f, 9.0f, 9.0f, 1.5f, tint, 0.7854f);
			float textX = kPartyLeft + 18.0f;
			textX += renderer->DrawLabel(calling.name, 15.0f, textX, rowY, tint) + 8.0f;
			renderer->DrawLabel(calling.role, 11.0f, textX, rowY - 2.0f, kLabel);

			// 체력
			const float ratio = std::max(0.0f, member->Health() / std::max(1.0f, member->MaxHealth()));
			const float barLeft = kPartyLeft + 18.0f;
			const float barWidth = 150.0f;
			const float barY = rowY - 26.0f;
			renderer->DrawRoundRect(barLeft + barWidth * 0.5f, barY, barWidth, 6.0f, 3.0f, Color{ 0.06f, 0.065f, 0.08f, 1.0f });
			if (ratio > 0.0f)
			{
				renderer->DrawRoundRect(barLeft + barWidth * ratio * 0.5f, barY, barWidth * ratio, 6.0f, 3.0f,
				                        (ratio < 0.35f) ? Color{ 0.85f, 0.25f, 0.2f, 1.0f } : tint);
			}
			const std::string health = std::to_string(static_cast<int>(member->Health() + 0.5f)) + "/"
			                         + std::to_string(static_cast<int>(member->MaxHealth() + 0.5f));
			DrawDigits(renderer, health, 11.0f, barLeft + barWidth + 8.0f, barY + 6.0f, kLabel);

			const std::string until = std::to_string(member->HiredUntil() + 1) + "번째 마을까지";
			renderer->DrawLabel(until, 11.0f, kPartyRight - kButtonWidth - 10.0f - renderer->MeasureLabel(until, 11.0f),
			                    rowY - 2.0f, kLabel);

			// 보내기
			const float buttonX = kPartyRight - kButtonWidth * 0.5f;
			const float buttonY = rowY - 14.0f;
			const bool pressed = (button == i);
			renderer->DrawRoundRect(buttonX, buttonY, kButtonWidth, kButtonHeight, 7.0f,
			                        pressed ? kHover : Color{ 0.30f, 0.26f, 0.20f, 1.0f });
			renderer->DrawRoundRect(buttonX, buttonY, kButtonWidth - 2.0f, kButtonHeight - 2.0f, 6.0f,
			                        pressed ? Color{ 0.22f, 0.17f, 0.10f, 1.0f } : Color{ 0.10f, 0.09f, 0.08f, 1.0f });
			const float labelWidth = renderer->MeasureLabel("보내기", 12.0f);
			renderer->DrawLabel("보내기", 12.0f, buttonX - labelWidth * 0.5f, buttonY + 8.0f,
			                    pressed ? kHover : kValue);
		}
	}

	// ── 끌고 있는 물건은 마우스를 따라 맨 위에 ──
	if (m_Drag.active && m_Drag.moved)
	{
		renderer->DrawCircle(m_MouseX, m_MouseY, 22.0f, Color{ 0.0f, 0.0f, 0.0f, 0.35f }, 0.0f, 6.0f);
		DrawItemIcon(renderer, m_Drag.kind, m_MouseX, m_MouseY, 40.0f);
	}
}
