#include "stdafx.h"
#include "Items.h"

#include <algorithm>

namespace
{
	const ItemInfo kItems[] =
	{
		{ "",       "",                                        0 },
		{ "회복약", "체력을 35% 채운다",                       10 },
		{ "횃불",   "선 자리에 세우면 꺼지지 않는 불이 된다",  10 },
		{ "기름병", "등불 기름을 절반 채운다",                 10 },
		{ "강화석", "무기를 한 단계 벼린다",                   5 },
	};
}

const ItemInfo& ItemOf(ItemKind kind)
{
	const int index = static_cast<int>(kind);
	const int count = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));
	return kItems[(index >= 0 && index < count) ? index : 0];
}

bool Inventory::CanAdd(ItemKind kind, int count) const
{
	if (kind == ItemKind::None || count <= 0)
	{
		return false;
	}
	const int stack = ItemOf(kind).stack;
	int room = 0;
	for (int i = 0; i < kSlots; ++i)
	{
		if (m_Slots[i].kind == kind)
		{
			room += stack - m_Slots[i].count;
		}
		else if (m_Slots[i].kind == ItemKind::None)
		{
			room += stack;
		}
	}
	return room >= count;
}

bool Inventory::Add(ItemKind kind, int count)
{
	if (!CanAdd(kind, count))
	{
		return false;
	}
	const int stack = ItemOf(kind).stack;

	// 이미 든 칸부터 채운다
	for (int i = 0; i < kSlots && count > 0; ++i)
	{
		if (m_Slots[i].kind == kind && m_Slots[i].count < stack)
		{
			const int put = std::min(count, stack - m_Slots[i].count);
			m_Slots[i].count += put;
			count -= put;
		}
	}
	for (int i = 0; i < kSlots && count > 0; ++i)
	{
		if (m_Slots[i].kind == ItemKind::None)
		{
			const int put = std::min(count, stack);
			m_Slots[i].kind = kind;
			m_Slots[i].count = put;
			count -= put;
		}
	}
	return true;
}

int Inventory::Count(ItemKind kind) const
{
	int total = 0;
	for (int i = 0; i < kSlots; ++i)
	{
		if (m_Slots[i].kind == kind)
		{
			total += m_Slots[i].count;
		}
	}
	return total;
}

bool Inventory::Remove(ItemKind kind, int count)
{
	if (kind == ItemKind::None || Count(kind) < count)
	{
		return false;
	}
	for (int i = kSlots - 1; i >= 0 && count > 0; --i)
	{
		if (m_Slots[i].kind != kind)
		{
			continue;
		}
		const int take = std::min(count, m_Slots[i].count);
		m_Slots[i].count -= take;
		count -= take;
		if (m_Slots[i].count <= 0)
		{
			m_Slots[i] = Slot();
		}
	}
	return true;
}

void Inventory::Move(int from, int to)
{
	if (from == to || from < 0 || to < 0 || from >= kSlots || to >= kSlots)
	{
		return;
	}
	Slot& source = m_Slots[from];
	Slot& target = m_Slots[to];
	if (source.kind == ItemKind::None)
	{
		return;
	}

	if (target.kind == source.kind)
	{
		const int put = std::min(source.count, ItemOf(target.kind).stack - target.count);
		target.count += put;
		source.count -= put;
		if (source.count <= 0)
		{
			source = Slot();
		}
		return;
	}
	std::swap(source, target);
}

void Inventory::Bind(int index, ItemKind kind)
{
	if (index < 0 || index >= kHotbar)
	{
		return;
	}
	for (int i = 0; i < kHotbar; ++i)
	{
		if (i != index && m_Hotbar[i] == kind && kind != ItemKind::None)
		{
			m_Hotbar[i] = m_Hotbar[index];   // 원래 있던 자리와 바꾼다
		}
	}
	m_Hotbar[index] = kind;
}

void Inventory::SwapHotkeys(int a, int b)
{
	if (a < 0 || b < 0 || a >= kHotbar || b >= kHotbar)
	{
		return;
	}
	std::swap(m_Hotbar[a], m_Hotbar[b]);
}

bool Inventory::HotbarEmpty() const
{
	for (int i = 0; i < kHotbar; ++i)
	{
		if (m_Hotbar[i] != ItemKind::None)
		{
			return false;
		}
	}
	return true;
}
