#include "stdafx.h"
#include "Progression.h"

#include <algorithm>
#include <cmath>

namespace Progression
{
	int SoulsToNextLevel(int level)
	{
		// 10, 15, 21, 27, 34 … 처음엔 금방, 갈수록 조금씩 멀어진다
		return 10 + static_cast<int>(std::pow(static_cast<float>(level - 1), 1.35f) * 5.0f + 0.5f);
	}

	float MaxHealth(int level)
	{
		return 100.0f + 15.0f * (level - 1);
	}

	float AttackRange(int level)
	{
		return std::min(390.0f, 270.0f + 6.0f * (level - 1));
	}

	int ProjectileCount(int level)
	{
		return std::min(4, 1 + level / 5);
	}

	float PickupRadius(int level)
	{
		return std::min(170.0f, 70.0f + 5.0f * (level - 1));
	}

	float MoveSpeed(int level)
	{
		return std::min(232.0f, 190.0f + 2.5f * (level - 1));
	}

	float AttackCooldown(int level, int weaponTier)
	{
		const float cooldown = 0.72f
		                     * std::pow(0.94f, static_cast<float>(level - 1))
		                     * std::pow(0.95f, static_cast<float>(weaponTier));
		return std::max(0.16f, cooldown);
	}

	float AttackDamage(int level, int weaponTier)
	{
		return (10.0f + 3.0f * (level - 1)) * (1.0f + 0.25f * weaponTier);
	}

	float ProjectileSpeed(int weaponTier)
	{
		return 520.0f + 40.0f * weaponTier;
	}

	int ProjectilePierce(int weaponTier)
	{
		return (weaponTier >= 5) ? 2 : (weaponTier >= 3 ? 1 : 0);
	}

	float LevelUpHealRatio()
	{
		return 0.3f;
	}
}
