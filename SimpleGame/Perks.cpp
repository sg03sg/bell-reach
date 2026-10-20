#include "stdafx.h"
#include "Perks.h"

#include "Progression.h"

namespace Perks
{
	namespace
	{
		// 순서가 Dialogue.cpp의 kWakeScripts와 같아야 한다.
		// 말을 건 사람과 도와주는 사람이 같아야 하기 때문이다.
		const Info kPerks[] =
		{
			{ "대장장이",    "벼린 불씨",   "무기 강화 +1 단계" },
			{ "광부",        "광맥의 눈",   "영혼석 +30%" },
			{ "수도승",      "기도",        "최대 체력 +25, 무적 시간 +0.3초" },
			{ "약초꾼",      "약초 주머니", "회복량 +60%" },
			{ "사냥꾼",      "멀리 보는 눈", "사거리 +70" },
			{ "필경사",      "지도",        "줍는 거리 +70" },
			{ "종지기 노인", "종을 다루는 법", "종 울리는 시간 -40%, 종탑 불빛 +25%" },
			{ "떠돌이 병사", "나란히",      "불씨 +1발" },
		};

		const int kCount = static_cast<int>(sizeof(kPerks) / sizeof(kPerks[0]));
	}

	int Count()
	{
		return kCount;
	}

	const Info& At(int index)
	{
		return kPerks[Wrap(index)];
	}

	int Wrap(int index)
	{
		int wrapped = index % kCount;
		if (wrapped < 0)
		{
			wrapped += kCount;
		}
		return wrapped;
	}

	void Set::Grant(int index)
	{
		const int slot = Wrap(index);
		const unsigned int bit = 1u << slot;
		if ((m_Joined & bit) != 0u)
		{
			return;   // 이미 있는 사람이다
		}

		m_Joined |= bit;
		m_Newest = slot;
		Rebuild();
	}

	bool Set::Has(int index) const
	{
		return (m_Joined & (1u << Wrap(index))) != 0u;
	}

	int Set::Count() const
	{
		int count = 0;
		for (unsigned int bits = m_Joined; bits != 0u; bits >>= 1)
		{
			count += static_cast<int>(bits & 1u);
		}
		return count;
	}

	//
	// 합류한 사람들의 효과를 처음부터 다시 더한다.
	//
	// 더할 때마다 조금씩 고치는 대신 매번 새로 쌓는 것은, 나중에 사람이
	// 떠나거나 효과가 바뀌어도 여기 한 곳만 맞으면 되기 때문이다.
	// 합류는 드문 일이라 비용은 문제가 되지 않는다.
	//
	void Set::Rebuild()
	{
		m_Mods = Modifiers();

		for (int i = 0; i < kCount; ++i)
		{
			if ((m_Joined & (1u << i)) == 0u)
			{
				continue;
			}

			switch (i)
			{
			case 0:   // 대장장이 — 무기를 벼린다
				m_Mods.weaponTierBonus += 1;
				break;
			case 1:   // 광부 — 돌 속의 영혼을 알아본다
				m_Mods.soulScale += 0.30f;
				break;
			case 2:   // 수도승 — 기도가 몸을 지킨다
				m_Mods.healthBonus += 25.0f;
				m_Mods.invulnerableBonus += 0.30f;
				break;
			case 3:   // 약초꾼 — 약을 짓는다
				m_Mods.healScale += 0.60f;
				break;
			case 4:   // 사냥꾼 — 어둠 너머를 본다
				m_Mods.rangeBonus += 70.0f;
				break;
			case 5:   // 필경사 — 지나온 길을 그려 둔다
				m_Mods.pickupBonus += 70.0f;
				break;
			case 6:   // 종지기 노인 — 종을 다룰 줄 안다
				m_Mods.ringScale *= 0.60f;
				m_Mods.beaconScale += 0.25f;
				break;
			case 7:   // 떠돌이 병사 — 나란히 서서 싸운다
				m_Mods.projectileBonus += 1;
				break;
			default:
				break;
			}
		}
	}
}
