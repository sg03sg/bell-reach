#include "stdafx.h"
#include "Professions.h"

#include <algorithm>
#include <cmath>

namespace
{
	const ProfessionInfo kProfessions[] =
	{
		//  이름       빛깔                        처음 돈 거래   꾸미는 순서
		//                                                   체력   돌    호위 품삯
		{ "주민",     { 0.62f, 0.52f, 0.40f },    8,   false,
		  { Decor::Planter, Decor::Bench, Decor::Banner, Decor::Lantern, Decor::Crates, Decor::Sign },
		  60.0f,  9.0f, 0, 0 },
		{ "상인",     { 0.86f, 0.62f, 0.24f },    25,  true,
		  { Decor::Sign, Decor::Crates, Decor::Lantern, Decor::Banner, Decor::Planter, Decor::Bench },
		  60.0f,  7.0f, 0, 0 },
		{ "사냥꾼",   { 0.44f, 0.58f, 0.32f },    6,   true,
		  { Decor::Sign, Decor::Banner, Decor::Crates, Decor::Lantern, Decor::Bench, Decor::Planter },
		  75.0f, 14.0f, 1, 5 },
		{ "의사",     { 0.80f, 0.82f, 0.86f },    15,  true,
		  { Decor::Sign, Decor::Lantern, Decor::Planter, Decor::Bench, Decor::Banner, Decor::Crates },
		  60.0f,  6.0f, 0, 0 },
		{ "정보상",   { 0.52f, 0.42f, 0.74f },    15,  true,
		  { Decor::Sign, Decor::Lantern, Decor::Banner, Decor::Bench, Decor::Planter, Decor::Crates },
		  60.0f,  7.0f, 0, 0 },
		{ "경비",     { 0.56f, 0.60f, 0.68f },    10,  false,
		  { Decor::Banner, Decor::Lantern, Decor::Crates, Decor::Bench, Decor::Planter, Decor::Sign },
		  90.0f, 11.0f, 0, 0 },
		{ "기사",     { 0.74f, 0.78f, 0.88f },    20,  false,
		  { Decor::Banner, Decor::Lantern, Decor::Sign, Decor::Bench, Decor::Crates, Decor::Planter },
		  130.0f, 18.0f, 3, 15 },
		{ "모험가",   { 0.62f, 0.40f, 0.30f },    10,  false,
		  { Decor::Crates, Decor::Banner, Decor::Lantern, Decor::Bench, Decor::Planter, Decor::Sign },
		  90.0f, 12.0f, 2, 8 },
		{ "전령",     { 0.30f, 0.52f, 0.62f },    10,  true,
		  { Decor::Sign, Decor::Banner, Decor::Lantern, Decor::Planter, Decor::Bench, Decor::Crates },
		  60.0f,  7.0f, 0, 0 },
	};
}

const ProfessionInfo& ProfessionOf(Profession profession)
{
	const int index = static_cast<int>(profession);
	const int count = static_cast<int>(sizeof(kProfessions) / sizeof(kProfessions[0]));
	return kProfessions[(index >= 0 && index < count) ? index : 0];
}

namespace
{
	const CallingInfo kCallings[] =
	{
		//  이름      갈래          고용 창의 한 줄
		//  빛깔                     체력    피해   거리    간격  피해배율 품삯
		{ "모험가", "",           "",
		  { 0.62f, 0.40f, 0.30f },   90.0f, 12.0f, 250.0f, 1.3f, 1.00f, 30 },
		{ "전사",   "근접 베기",   "붙어서 벤다. 칼끝 둘레의 적도 맞는다",
		  { 0.72f, 0.30f, 0.24f },  140.0f, 20.0f,  46.0f, 0.8f, 0.80f, 30 },
		{ "궁수",   "원거리",      "멀리서 쏜다. 적이 붙으면 물러난다",
		  { 0.40f, 0.56f, 0.30f },   90.0f, 17.0f, 400.0f, 1.0f, 1.00f, 30 },
		{ "힐러",   "회복",        "다친 사람을 고친다. 한가하면 빛 조각을 던진다",
		  { 0.86f, 0.84f, 0.70f },   90.0f,  7.0f, 320.0f, 1.6f, 1.00f, 40 },
		{ "마법사", "원거리 광역", "무리 한가운데 불덩이를 떨군다",
		  { 0.44f, 0.36f, 0.78f },   80.0f, 26.0f, 340.0f, 3.0f, 1.00f, 45 },
		{ "방패병", "도발 · 막기", "둘레의 적을 제게로 끈다. 맞아도 덜 아프다",
		  { 0.50f, 0.56f, 0.66f },  200.0f, 10.0f,  44.0f, 1.2f, 0.55f, 35 },
	};
}

const CallingInfo& CallingOf(Calling calling)
{
	const int index = static_cast<int>(calling);
	const int count = static_cast<int>(sizeof(kCallings) / sizeof(kCallings[0]));
	return kCallings[(index >= 0 && index < count) ? index : 0];
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

	int HireFee(Calling calling, int village)
	{
		return CallingOf(calling).fee + 10 * std::max(0, village);
	}
}

int RequiredEscort(int fromVillage, int toVillage)
{
	// 첫 마을과 둘째 마을 사이는 기사 하나(3)면 된다. 그 뒤로는 두 마을마다 하나씩 더.
	const int deeper = std::max(fromVillage, toVillage);
	return 2 + (deeper + 1) / 2;
}
