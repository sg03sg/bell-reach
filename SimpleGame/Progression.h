#pragma once

//
// 성장 공식.
//
// 레벨은 영혼석을 주워 올린다. 적을 쓰러뜨리는 것만으로는 오르지 않는다 —
// 쓰러뜨린 자리에 떨어진 영혼을 거둬야 한다. 어둠 속에 떨어진 영혼석을
// 주우러 가는 짧은 모험이 전투 사이사이에 생긴다.
//
// 레벨과 무기 강화 단계가 어떤 능력치로 이어지는지는 전부 여기 있다.
// 밸런스를 만질 때 이 파일 하나만 보면 된다.
//
namespace Progression
{
	const int kMaxWeaponTier = 5;

	// 다음 레벨까지 필요한 영혼
	int SoulsToNextLevel(int level);

	// ── 레벨이 올리는 것 ──
	float MaxHealth(int level);              // 최대 체력
	float AttackRange(int level);            // 자동 조준이 닿는 거리(px)
	int   ProjectileCount(int level);        // 한 번에 쏘는 불씨 수. 5, 10, 15레벨에 하나씩
	float PickupRadius(int level);           // 떨어진 것이 저절로 끌려오는 거리(px)
	float MoveSpeed(int level);              // 걷는 속도(px/s)

	// ── 레벨과 무기 강화가 함께 올리는 것 ──
	float AttackCooldown(int level, int weaponTier);   // 발사 간격(초)
	float AttackDamage(int level, int weaponTier);     // 불씨 하나의 피해

	// ── 무기 강화만 올리는 것 ──
	float ProjectileSpeed(int weaponTier);   // 불씨 속도(px/s)
	int   ProjectilePierce(int weaponTier);  // 불씨가 꿰뚫고 지나가는 적 수. 3, 5단계에 하나씩

	// 레벨이 오를 때 되찾는 체력의 비율
	float LevelUpHealRatio();
}
