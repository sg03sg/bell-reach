#include "stdafx.h"
#include "Combat.h"

namespace
{
	//                          이름            체력  속도  크기  접촉  간격  영혼 값  회복 무기 자석
	const EnemyType kWraith = { "떠도는 그림자",  26.0f, 118.0f, 11.0f,  8.0f, 0.8f,   1, 1,    6,   2,   3 };
	const EnemyType kHusk   = { "삼켜진 자",     70.0f,  72.0f, 14.0f, 16.0f, 1.1f,   2, 2,   14,   5,   4 };
	const EnemyType kWarden = { "종탑지기",     520.0f,  80.0f, 26.0f, 22.0f, 1.0f,  10, 4,  100, 100,  60 };
}

const EnemyType& EnemyTypeOf(EnemyKind kind)
{
	switch (kind)
	{
	case EnemyKind::Husk:   return kHusk;
	case EnemyKind::Warden: return kWarden;
	default:                return kWraith;
	}
}
