#include "stdafx.h"
#include "Professions.h"

#include <algorithm>
#include <cmath>

namespace
{
	const ProfessionInfo kProfessions[] =
	{
		//  이름       빛깔                        처음 돈 거래   꾸미는 순서
		{ "주민",     { 0.62f, 0.52f, 0.40f },    8,   false,
		  { Decor::Planter, Decor::Bench, Decor::Banner, Decor::Lantern, Decor::Crates, Decor::Sign } },
		{ "상인",     { 0.86f, 0.62f, 0.24f },    25,  true,
		  { Decor::Sign, Decor::Crates, Decor::Lantern, Decor::Banner, Decor::Planter, Decor::Bench } },
		{ "사냥꾼",   { 0.44f, 0.58f, 0.32f },    6,   true,
		  { Decor::Sign, Decor::Banner, Decor::Crates, Decor::Lantern, Decor::Bench, Decor::Planter } },
		{ "의사",     { 0.80f, 0.82f, 0.86f },    15,  true,
		  { Decor::Sign, Decor::Lantern, Decor::Planter, Decor::Bench, Decor::Banner, Decor::Crates } },
		{ "정보상",   { 0.52f, 0.42f, 0.74f },    15,  true,
		  { Decor::Sign, Decor::Lantern, Decor::Banner, Decor::Bench, Decor::Planter, Decor::Crates } },
		{ "경비",     { 0.56f, 0.60f, 0.68f },    10,  false,
		  { Decor::Banner, Decor::Lantern, Decor::Crates, Decor::Bench, Decor::Planter, Decor::Sign } },
	};
}

const ProfessionInfo& ProfessionOf(Profession profession)
{
	const int index = static_cast<int>(profession);
	const int count = static_cast<int>(sizeof(kProfessions) / sizeof(kProfessions[0]));
	return kProfessions[(index >= 0 && index < count) ? index : 0];
}

namespace Prices
{
	int DoctorFee(float health, float maxHealth)
	{
		if (maxHealth <= 0.0f || health >= maxHealth)
		{
			return 0;
		}
		const float missing = 1.0f - health / maxHealth;
		return std::max(3, static_cast<int>(std::ceil(missing * 10.0f)) * 2);
	}

	int Bounty(bool hunter, bool warden)
	{
		const int base = warden ? 30 : 2;
		return hunter ? base * 3 : base;
	}
}
