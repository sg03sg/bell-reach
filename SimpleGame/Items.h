#pragma once

//
// 가방과 단축칸.
//
// 상인과 사냥꾼에게서 산 것은 바로 쓰이지 않고 가방에 들어간다. 가방은
// I로 열고, 단축칸(1~9)에 끌어다 놓으면 숫자 키 하나로 꺼내 쓴다.
//
// 단축칸에는 물건 자체가 아니라 "어떤 물건"을 걸어 둔다. 가방에 그 물건이
// 몇 개 있든 단축칸은 같은 칸을 가리키고, 다 쓰면 빈 그림으로 남는다 —
// 다시 사 오면 그 자리에서 바로 쓸 수 있다.
//
// 그리기와 마우스는 Game(InventoryUI.cpp)이 맡는다. 여기는 셈만 한다.
//
enum class ItemKind : unsigned char
{
	None,
	HealPack,     // 회복약 — 체력 35%
	Torch,        // 횃불 — 그 자리에 세우면 꺼지지 않는다
	Oil,          // 기름병 — 등불 기름 절반
	WeaponStone,  // 강화석 — 무기 강화 한 단계
	Count
};

struct ItemInfo
{
	const char* name;
	const char* note;    // 가방에서 가리키면 보이는 한 줄
	int stack;           // 한 칸에 쌓이는 수
};

const ItemInfo& ItemOf(ItemKind kind);

class Inventory
{
public:
	static const int kColumns = 5;
	static const int kRows = 4;
	static const int kSlots = kColumns * kRows;
	static const int kHotbar = 9;

	struct Slot
	{
		ItemKind kind = ItemKind::None;
		int count = 0;
	};

	// 같은 것이 든 칸에 먼저 쌓고, 넘치면 빈 칸에. 다 못 넣으면 하나도 넣지 않고 false.
	bool Add(ItemKind kind, int count = 1);
	bool CanAdd(ItemKind kind, int count = 1) const;

	// 뒤쪽 칸부터 뺀다(앞쪽에 정리해 둔 칸이 오래 남도록). 모자라면 빼지 않고 false.
	bool Remove(ItemKind kind, int count = 1);
	int Count(ItemKind kind) const;

	const Slot& At(int index) const { return m_Slots[index]; }

	// 칸을 옮긴다. 같은 것이면 합치고, 다르면 바꾼다.
	void Move(int from, int to);

	// ── 단축칸 ──
	ItemKind Hotkey(int index) const { return m_Hotbar[index]; }
	// 걸어 둔다. 같은 물건이 다른 칸에 걸려 있었으면 그 칸과 자리를 바꾼다.
	void Bind(int index, ItemKind kind);
	void SwapHotkeys(int a, int b);
	void Unbind(int index) { m_Hotbar[index] = ItemKind::None; }
	bool HotbarEmpty() const;

private:
	Slot m_Slots[kSlots];
	ItemKind m_Hotbar[kHotbar] = {};
};
