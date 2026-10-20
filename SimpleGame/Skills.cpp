#include "stdafx.h"
#include "Skills.h"

#include "Perks.h"

namespace Skills
{
	namespace
	{
		// 순서가 Dialogue.cpp의 kWakeScripts, Perks.cpp의 kPerks와 같아야 한다.
		const Info kSkills[] =
		{
			{ "대장장이",    "망치 내려치기", "근접 광역",
			  "둘레의 적을 내려쳐 밀어낸다",               3.5f,  95.0f, true, { 1.00f, 0.55f, 0.22f } },
			{ "광부",        "폭약",          "원거리 광역",
			  "적에게 폭약을 던져 둘레를 날린다",          6.0f, 300.0f, true, { 0.98f, 0.74f, 0.30f } },
			{ "수도승",      "기도",          "회복 · 보호",
			  "체력 18% 회복, 1.5초 무적",                 9.0f,   0.0f, true, { 1.00f, 0.90f, 0.58f } },
			{ "약초꾼",      "독초 연기",     "장판",
			  "5초간 독 연기. 안의 적은 피해를 입고 느려진다", 7.0f, 260.0f, true, { 0.58f, 0.86f, 0.36f } },
			{ "사냥꾼",      "화살",          "원거리 단일",
			  "멀리 있는 적을 하나씩 꿰뚫는다",            1.3f, 380.0f, false, { 0.78f, 0.86f, 0.52f } },
			{ "필경사",      "봉인의 글",     "둔화 장판",
			  "4.5초간 둘레의 적을 크게 느리게 한다",       8.0f, 170.0f, true, { 0.64f, 0.58f, 1.00f } },
			{ "종지기 노인", "종소리",        "광역 밀쳐내기",
			  "둘레의 적을 멀리 밀어낸다",                 6.0f, 140.0f, true, { 0.95f, 0.76f, 0.40f } },
			{ "떠돌이 병사", "베기",          "근접 연타",
			  "가까운 적을 빠르게 벤다",                   0.9f, 110.0f, false, { 0.82f, 0.90f, 1.00f } },
		};

		const int kCount = static_cast<int>(sizeof(kSkills) / sizeof(kSkills[0]));
	}

	int Count()
	{
		return kCount;
	}

	const Info& At(int profession)
	{
		return kSkills[Perks::Wrap(profession)];
	}
}
