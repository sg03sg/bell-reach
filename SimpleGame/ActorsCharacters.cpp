#include "stdafx.h"
#include "Actors.h"

#include "Art.h"
#include "Game.h"
#include "Input.h"
#include "Professions.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Skills.h"

#include <algorithm>
#include <cmath>

using namespace Art;

namespace
{
	const float kPlayerHalfSize = 9.0f;
	const float kInvulnerableTime = 0.55f;

	const float kColorHurt[3] = { 1.00f, 0.36f, 0.30f };
	const float kColorGold[3] = { 1.00f, 0.82f, 0.40f };
}

// ═══ 플레이어 ═════════════════════════════════════════════════════
PlayerActor::PlayerActor(float worldX, float worldY)
	: Actor("Player")
{
	SetPosition(worldX, worldY);
	m_Combat.health = Progression::MaxHealth(m_Combat.level);
	m_Combat.healthShown = m_Combat.health;
	m_Path.push_back({ worldX, worldY, 0.0f });
}

float PlayerActor::MaxHealth() const
{
	return Progression::MaxHealth(m_Combat.level) + m_Mods.healthBonus;
}

bool PlayerActor::GetLight(float& radius, bool& leavesTrail) const
{
	// 기름이 줄면 반경도 줄어든다. 꺼지기 전에 먼저 세계가 좁아진다.
	if (m_Oil <= 0.0f)
	{
		return false;
	}
	radius = kLanternRadius * (0.35f + 0.65f * m_Oil);
	leavesTrail = true;   // 등불만 발자국을 남긴다
	return true;
}

void PlayerActor::PlaceAt(float worldX, float worldY)
{
	SetPosition(worldX, worldY);
	m_Path.clear();
	m_PathLength = 0.0f;
	m_Path.push_back({ worldX, worldY, 0.0f });
	m_Moving = false;
}

void PlayerActor::Walk(const ActorContext& context)
{
	const Input& input = *context.input;

	float moveX = 0.0f;
	float moveY = 0.0f;
	if (input.MoveLeft())  { moveX -= 1.0f; }
	if (input.MoveRight()) { moveX += 1.0f; }
	if (input.MoveUp())    { moveY -= 1.0f; }
	if (input.MoveDown())  { moveY += 1.0f; }

	// 대각선이 더 빠르지 않도록 정규화한다.
	const float length = std::sqrt(moveX * moveX + moveY * moveY);
	m_Moving = length > 0.0f;
	if (moveX != 0.0f)
	{
		m_Facing = (moveX > 0.0f) ? 1.0f : -1.0f;
	}
	if (!m_Moving)
	{
		return;
	}

	const float step = Progression::MoveSpeed(m_Combat.level) * context.deltaSeconds;
	const float dx = moveX / length * step;
	const float dy = moveY / length * step;

	float x = WorldX();
	float y = WorldY();

	// 축을 나눠서 옮겨야 벽에 부딪혔을 때 미끄러진다.
	auto blocked = [&](float px, float py)
	{
		return context.game->IsBodyBlocked(px, py, kPlayerHalfSize);
	};
	if (!blocked(x + dx, y)) { x += dx; }
	if (!blocked(x, y + dy)) { y += dy; }

	SetPosition(x, y);
}

void PlayerActor::RecordTrail()
{
	const PathPoint& last = m_Path.back();
	const float moved = Distance(WorldX(), WorldY(), last.x, last.y);
	if (moved < kPathStep)
	{
		return;
	}

	m_PathLength += moved;
	m_Path.push_back({ WorldX(), WorldY(), m_PathLength });

	// 필요한 만큼만 들고 있는다. 무한 맵을 걸어도 자취가 무한히 늘지 않는다.
	while (m_Path.size() > 2 && m_PathLength - m_Path.front().travelled > kPathKeep)
	{
		m_Path.pop_front();
	}
}

bool PlayerActor::TrailPoint(float lag, float& outX, float& outY) const
{
	if (m_Path.size() < 2)
	{
		return false;
	}

	const float want = m_PathLength - lag;
	if (want <= m_Path.front().travelled)
	{
		outX = m_Path.front().x;
		outY = m_Path.front().y;
		return true;
	}

	for (size_t i = m_Path.size() - 1; i > 0; --i)
	{
		const PathPoint& before = m_Path[i - 1];
		const PathPoint& after = m_Path[i];
		if (before.travelled <= want && want <= after.travelled)
		{
			const float span = after.travelled - before.travelled;
			const float t = (span > 0.0f) ? (want - before.travelled) / span : 0.0f;
			outX = Mix(before.x, after.x, t);
			outY = Mix(before.y, after.y, t);
			return true;
		}
	}
	return false;
}

void PlayerActor::Fire(const ActorContext& context)
{
	if (m_Combat.attackCooldown > 0.0f || !context.input->Attack())
	{
		return;
	}

	const int level = m_Combat.level;

	// 대장장이가 합류해 있으면 무기가 한 단계 더 벼려진 셈으로 친다.
	const int tier = std::min(Progression::kMaxWeaponTier,
	                          m_Combat.weaponTier + m_Mods.weaponTierBonus);
	const float range = Progression::AttackRange(level) + m_Mods.rangeBonus;

	// 사거리 안의 적을 가까운 순서로 고른다 — 자동 조준이다.
	std::vector<std::pair<float, EnemyActor*> > targets;
	const std::vector<EnemyActor*>& enemies = context.game->Enemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		if (enemies[i]->IsDying())
		{
			continue;
		}
		const float distance = Distance(WorldX(), WorldY(), enemies[i]->WorldX(), enemies[i]->BodyCenterY());
		if (distance <= range)
		{
			targets.push_back(std::make_pair(distance, enemies[i]));
		}
	}
	std::sort(targets.begin(), targets.end(),
	          [](const std::pair<float, EnemyActor*>& a, const std::pair<float, EnemyActor*>& b)
	          {
	          	return a.first < b.first;
	          });

	// 사거리 안에 적이 없으면 마을 사람을 겨냥한다. 마을 안에서도 F가
	// 헛돌지 않게 하려는 것이다. 적이 하나라도 있으면 사람은 보지 않으므로,
	// 싸우는 도중에 엉뚱한 사람을 맞히지는 않는다.
	float townX = 0.0f;
	float townY = 0.0f;
	bool hasTownTarget = false;
	if (targets.empty())
	{
		float nearest = range;
		const std::vector<VillagerActor*>& villagers = context.game->Villagers();
		for (size_t i = 0; i < villagers.size(); ++i)
		{
			if (villagers[i]->IsDead() || villagers[i]->IsHired())   // 함께 다니는 사람은 겨누지 않는다
			{
				continue;
			}
			const float distance = Distance(WorldX(), WorldY(),
			                                villagers[i]->WorldX(), villagers[i]->BodyCenterY());
			if (distance < nearest)
			{
				nearest = distance;
				townX = villagers[i]->WorldX();
				townY = villagers[i]->BodyCenterY();
				hasTownTarget = true;
			}
		}
	}

	const int shots = Progression::ProjectileCount(level) + m_Mods.projectileBonus;
	const float speed = Progression::ProjectileSpeed(tier);

	// 손에 든 등불에서 불씨가 떠난다
	const float originX = WorldX() + m_Facing * 10.0f;
	const float originY = WorldY() - 10.0f;

	for (int s = 0; s < shots; ++s)
	{
		float directionX = m_Facing;
		float directionY = 0.0f;
		int targetId = -1;

		if (!targets.empty())
		{
			// 과녁이 여럿이면 하나씩 나눠 맡고, 모자라면 같은 과녁에 부채꼴로 쏜다.
			EnemyActor* target = targets[s % targets.size()].second;
			directionX = target->WorldX() - originX;
			directionY = target->BodyCenterY() - originY;
			targetId = target->Id();
		}
		else if (hasTownTarget)
		{
			// 사람에게는 유도하지 않는다(targetId를 주지 않는다). 겨냥만 한다.
			directionX = townX - originX;
			directionY = townY - originY;
		}

		const float length = std::sqrt(directionX * directionX + directionY * directionY);
		if (length > 0.001f)
		{
			directionX /= length;
			directionY /= length;
		}

		const int spreadIndex = (targets.empty() || static_cast<int>(targets.size()) < shots) ? s : 0;
		const float spread = (spreadIndex - (shots - 1) * 0.5f) * 0.14f;
		if (spread != 0.0f)
		{
			const float c = std::cos(spread);
			const float n = std::sin(spread);
			const float turnedX = directionX * c - directionY * n;
			const float turnedY = directionX * n + directionY * c;
			directionX = turnedX;
			directionY = turnedY;
		}

		context.scene->Spawn(std::unique_ptr<Actor>(new ProjectileActor(
			originX, originY, directionX * speed, directionY * speed,
			Progression::AttackDamage(level, tier), Progression::ProjectilePierce(tier),
			range * 1.15f, targetId)));
	}

	m_Combat.attackCooldown = Progression::AttackCooldown(level, tier);
}

void PlayerActor::OnUpdate(const ActorContext& context)
{
	const float deltaSeconds = context.deltaSeconds;

	// 되찾은 사람들이 주는 보정을 이번 프레임 값으로 맞춘다.
	m_Mods = context.game->Mods();

	m_Combat.invulnerable = std::max(0.0f, m_Combat.invulnerable - deltaSeconds);
	m_Combat.hurtFlash = std::max(0.0f, m_Combat.hurtFlash - deltaSeconds * 2.5f);
	m_Combat.magnetTime = std::max(0.0f, m_Combat.magnetTime - deltaSeconds);
	m_Combat.levelUpGlow = std::max(0.0f, m_Combat.levelUpGlow - deltaSeconds * 0.8f);
	m_Combat.shield = std::max(0.0f, m_Combat.shield - deltaSeconds);
	m_Combat.attackCooldown = std::max(0.0f, m_Combat.attackCooldown - deltaSeconds);

	// 체력바 뒤로 따라오는 흰 막대. 한 번에 깎인 양이 눈에 보이게 천천히 줄어든다.
	m_Combat.healthShown = (m_Combat.healthShown > m_Combat.health)
		? std::max(m_Combat.health, m_Combat.healthShown - deltaSeconds * 60.0f)
		: m_Combat.health;

	// 쓰러져 있는 동안에는 움직이지도 쏘지도 않는다.
	if (context.game->IsRespawning())
	{
		m_Moving = false;
		return;
	}

	Walk(context);
	RecordTrail();
	Fire(context);

	// 기름은 계속 준다. 이미 되찾은 구역 안에서는 다시 채운다 —
	// 밝혀 놓은 땅이 보급선이 되는 셈이다.
	if (context.game->BeaconBrightness(WorldX(), WorldY()) > 0.0f)
	{
		m_Oil = std::min(1.0f, m_Oil + deltaSeconds / 12.0f);
	}
	else
	{
		m_Oil = std::max(0.0f, m_Oil - deltaSeconds / kOilSeconds);
	}
}

void PlayerActor::OnDraw(const DrawContext& context) const
{
	DrawPlayer(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	           m_Oil, m_Facing, m_Moving, context.time);

	// 수도승의 기도가 씌운 보호막. 꺼질 무렵엔 깜빡인다.
	if (m_Combat.shield > 0.0f)
	{
		const float strength = (m_Combat.shield > 0.4f) ? 1.0f
		                     : (0.5f + 0.5f * std::sin(context.time * 40.0f));
		const Color bubble = { kGold[0], kGold[1], kGold[2], 0.16f * strength };
		const Color rim = { kGold[0], kGold[1], kGold[2], 0.55f * strength };
		const float x = context.ToRenderX(WorldX());
		const float y = context.ToRenderY(WorldY());
		context.renderer->DrawCircle(x, y + 20.0f, 27.0f, bubble, 0.5f * strength, 6.0f);
		context.renderer->DrawEllipse(x, y + 1.0f, 18.0f, 6.0f, rim, 0.0f, 1.0f * strength, 2.0f);
	}

	// 레벨이 오른 순간, 발밑에서 영혼의 빛이 번져 나간다.
	if (m_Combat.levelUpGlow > 0.0f)
	{
		const float glow = m_Combat.levelUpGlow;
		const Color ring = { kSoulColor[0], kSoulColor[1], kSoulColor[2], glow * 0.35f };
		context.renderer->DrawCircle(context.ToRenderX(WorldX()), context.ToRenderY(WorldY()) + 14.0f,
		                             22.0f + (1.0f - glow) * 46.0f, ring, glow * 2.0f, 8.0f);
	}
}

void PlayerActor::TakeDamage(const ActorContext& context, float amount, float fromX, float fromY)
{
	if (m_Combat.invulnerable > 0.0f || context.game->IsRespawning())
	{
		return;
	}

	m_Combat.health -= amount;
	// 수도승이 합류해 있으면 맞고 난 뒤 몸을 추스를 틈이 더 길다.
	m_Combat.invulnerable = kInvulnerableTime + m_Mods.invulnerableBonus;
	m_Combat.hurtFlash = 1.0f;

	context.game->SpawnFloatingText(WorldX(), WorldY() - 44.0f, std::string(),
	                                static_cast<int>(amount + 0.5f), kColorHurt, 15.0f);

	// 맞은 반대쪽으로 조금 밀려난다. 벽은 뚫지 않는다.
	float awayX = WorldX() - fromX;
	float awayY = WorldY() - fromY;
	const float length = std::sqrt(awayX * awayX + awayY * awayY);
	if (length > 0.001f)
	{
		const float pushX = awayX / length * 14.0f;
		const float pushY = awayY / length * 14.0f;
		float x = WorldX();
		float y = WorldY();
		if (!context.game->IsBodyBlocked(x + pushX, y, kPlayerHalfSize)) { x += pushX; }
		if (!context.game->IsBodyBlocked(x, y + pushY, kPlayerHalfSize)) { y += pushY; }
		SetPosition(x, y);
	}

	if (m_Combat.health <= 0.0f)
	{
		m_Combat.health = 0.0f;
		context.game->RequestRespawn();
	}
}

void PlayerActor::GainSouls(const ActorContext& context, int amount)
{
	// 광부가 합류해 있으면 같은 돌에서 더 많은 영혼을 알아본다.
	m_Combat.souls += static_cast<int>(amount * m_Mods.soulScale + 0.5f);

	while (m_Combat.souls >= Progression::SoulsToNextLevel(m_Combat.level))
	{
		m_Combat.souls -= Progression::SoulsToNextLevel(m_Combat.level);

		const float previousMax = MaxHealth();
		++m_Combat.level;
		const float newMax = MaxHealth();

		// 늘어난 최대 체력만큼 채우고, 거기에 조금 더 회복한다
		m_Combat.health = std::min(newMax, m_Combat.health + (newMax - previousMax)
		                                   + newMax * Progression::LevelUpHealRatio()
		                                     * m_Mods.healScale);
		m_Combat.levelUpGlow = 1.0f;
		context.game->SpawnFloatingText(WorldX(), WorldY() - 60.0f, "레벨 업!", -1, kColorGold, 20.0f);
	}
}

bool PlayerActor::SpendGold(int amount)
{
	if (m_Combat.gold < amount)
	{
		return false;
	}
	m_Combat.gold -= amount;
	return true;
}

void PlayerActor::Shield(float seconds)
{
	m_Combat.invulnerable = std::max(m_Combat.invulnerable, seconds);
	m_Combat.shield = std::max(m_Combat.shield, seconds);
}

void PlayerActor::Heal(float amount)
{
	// 약초꾼이 합류해 있으면 같은 약이 더 듣는다.
	m_Combat.health = std::min(MaxHealth(), m_Combat.health + amount * m_Mods.healScale);
}

void PlayerActor::AddWeaponTier()
{
	m_Combat.weaponTier = std::min(Progression::kMaxWeaponTier, m_Combat.weaponTier + 1);
}

void PlayerActor::StartMagnet(float seconds)
{
	m_Combat.magnetTime = seconds;
}

// ═══ 동행자 ═══════════════════════════════════════════════════════
CompanionActor::CompanionActor(float worldX, float worldY, int regionX, int regionY, int profession)
	: Actor("Companion")
	, m_RegionX(regionX)
	, m_RegionY(regionY)
	, m_Profession(Perks::Wrap(profession))
{
	SetPosition(worldX, worldY);
}

void CompanionActor::OnUpdate(const ActorContext& context)
{
	PlayerActor* player = context.player;
	if (player == NULL)
	{
		return;
	}

	// 어둠 속에서는 다시 굳어가고, 빛 속에서는 돌아온다.
	// 침식도가 그대로 색과 자세가 되므로 따로 표시할 것이 없다.
	const float brightness = context.game->TotalBrightness(WorldX(), WorldY());
	m_Darkness += (brightness >= kRevealAt)
		? -context.deltaSeconds / RecoverSeconds()
		: context.deltaSeconds / ErodeSeconds();
	m_Darkness = std::max(0.0f, std::min(1.0f, m_Darkness));

	m_CastGlow = std::max(0.0f, m_CastGlow - context.deltaSeconds * 2.5f);
	m_FacingHold = std::max(0.0f, m_FacingHold - context.deltaSeconds);

	// 완전히 굳으면 그 자리에 멈춘다. 죽는 것이 아니라, 빛을 되돌려 비추면 깨어난다.
	// 굳은 사람은 싸우지도 않는다.
	if (IsFrozenAgain())
	{
		m_Walking = false;
		return;
	}

	// ── 함께 싸운다 ──
	// 어둠에 잠겨 갈수록 손이 굳어 다음 스킬이 늦게 돈다. 그래서 동행자를
	// 빛 안에 두고 싸우는 쪽이 낫다 — 등불을 들고 다니는 이유가 하나 더 생긴다.
	if (!context.game->IsRespawning())
	{
		m_SkillTimer = std::max(0.0f, m_SkillTimer - context.deltaSeconds * (1.0f - m_Darkness * 0.5f));
		if (m_SkillTimer <= 0.0f && UseSkill(context))
		{
			m_SkillTimer = Skills::At(m_Profession).cooldown;
			m_CastGlow = 1.0f;
		}
	}

	// 길찾기를 하지 않는다. 플레이어가 조금 전에 서 있던 자리를 밟을 뿐이라
	// 벽에 낄 수가 없다.
	float targetX = 0.0f;
	float targetY = 0.0f;
	if (!player->TrailPoint(kFollowLag, targetX, targetY))
	{
		return;
	}

	const float dx = targetX - WorldX();
	const float dy = targetY - WorldY();
	const float distance = std::sqrt(dx * dx + dy * dy);
	m_Walking = distance > kFollowLag * 0.6f && player->IsMoving();

	if (distance < 0.5f)
	{
		return;
	}

	// 굳어갈수록 발이 무거워진다. 너무 벌어지면 뛴다.
	float speed = 210.0f * (1.0f - m_Darkness * 0.45f);
	if (distance > 90.0f)
	{
		speed *= 2.2f;
	}

	const float step = std::min(distance, speed * context.deltaSeconds);
	SetPosition(WorldX() + dx / distance * step, WorldY() + dy / distance * step);
}

void CompanionActor::OnDraw(const DrawContext& context) const
{
	const float x = context.ToRenderX(WorldX());
	const float y = context.ToRenderY(WorldY());
	const float facing = (m_FacingHold > 0.0f) ? m_Facing
	                   : ((context.game->PlayerX() >= WorldX()) ? 1.0f : -1.0f);

	// 스킬을 막 썼으면 발밑에서 제 빛깔이 번진다
	if (m_CastGlow > 0.0f)
	{
		const float* tint = Skills::At(m_Profession).tint;
		const Color glow = { tint[0], tint[1], tint[2], 0.45f * m_CastGlow };
		context.renderer->DrawEllipse(x, y, 20.0f, 8.0f, glow, 0.0f, 1.4f * m_CastGlow, 5.0f);
	}

	DrawCompanion(context.renderer, x, y, 1.0f - m_Darkness, facing, m_Walking, context.time);
}

// ═══ 마을 사람 ═════════════════════════════════════════════════════
//
// 첫 마을에서 제 일을 하며 사는 사람들. 무엇을 할지는 행동 트리가 정한다.
// 마디에 담는 함수는 상태를 갖지 않고 칠판(Blackboard)만 읽고 쓴다.
//
// 마을은 저절로 돌아간다.
//   · 사냥꾼과 경비는 문 앞에서 어둠을 쫓고, 마지막 한 방을 넣으면 현상금을 받는다
//   · 주민은 일해서 조금씩 벌고, 돈이 있으면 상인에게 장을 본다
//   · 다치면 의사에게 가서 돈을 내고 치료받는다
//   · 돈이 모이면 제 집 둘레에 간판 · 등 · 화분 · 깃발을 놓는다
//   · 가난한 좀도둑은 부자에게 몰래 다가가 돈을 훔친다. 들키면 당한 사람과
//     경비가 쫓아가고, 도둑은 한 대 맞으면 맞서다가 두 대째에 돈을 내놓는다
//
// 누가 먼저 플레이어에게 맞으면 원한을 품고, 둘레 사람들도 함께 돌을 든다.

namespace
{
	const float kVillagerHalfSize = 8.0f;
	const float kNoticeRange = 96.0f;      // 이만큼 다가오면 하던 일을 멈추고 쳐다본다
	const float kArriveRange = 12.0f;      // 목적지에 이만큼 닿으면 도착으로 친다
	const float kGrudgeSeconds = 14.0f;    // 원한이 식기까지
	const float kRallyRange = 300.0f;      // 이 안의 사람들이 함께 원한을 품는다
	// 어둠에게 던지는 돌의 세기는 직업마다 다르다(Professions.cpp의 stoneDamage)
	const float kStoneToPlayer = 4.0f;     // 원한을 품고 던지는 돌
	const float kStoneToVillager = 3.0f;   // 마을 사람끼리 다툴 때 — 아프기만 하다
	const float kStoneSpeed = 360.0f;
	const float kQuarrelSeconds = 12.0f;   // 도둑을 쫓는 시간
	const float kStealReach = 26.0f;       // 이만큼 붙어야 주머니를 턴다
	const float kGuardNotice = 260.0f;     // 경비가 이 안에 있으면 도둑질은 반드시 들킨다
	const float kAngerRed[3] = { 0.92f, 0.20f, 0.15f };
	const float kQuarrelOrange[3] = { 1.00f, 0.62f, 0.20f };

	// ── 파티(고용한 모험가) ──
	const float kPartyNotice = 420.0f;     // 이 안의 적을 알아본다
	const float kPartyLeash = 460.0f;      // 플레이어에게서 이보다 먼 적은 쫓지 않는다
	const float kPartyCatchUp = 720.0f;    // 이보다 떨어지면 플레이어의 발자취로 옮겨 붙는다
	const float kPartyLag = 80.0f;         // 맨 앞 사람이 뒤처지는 거리. 동행자(46)보다 뒤에 선다
	const float kPartySpacing = 30.0f;     // 줄 선 차례마다 이만큼 더 뒤에

	// 둘레의 적을 한꺼번에 친다. 목록을 베껴 두고 도는 것은, 맞아 죽은 적이
	// 전리품을 떨구며 장면에 무언가를 놓아도 순회가 흔들리지 않게 하려는 것이다.
	int StrikeAround(const ActorContext& context, float x, float y, float radius, float damage, float push)
	{
		const std::vector<EnemyActor*> enemies = context.game->Enemies();
		int hits = 0;
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			EnemyActor* enemy = enemies[i];
			if (enemy->IsDying())
			{
				continue;
			}
			const float dx = enemy->WorldX() - x;
			const float dy = enemy->BodyCenterY() - y;
			const float distance = std::sqrt(dx * dx + dy * dy);
			if (distance > radius + enemy->Radius())
			{
				continue;
			}
			enemy->TakeDamage(context, damage, x, y);
			if (push > 0.0f && distance > 0.001f)
			{
				enemy->Push(dx / distance * push, dy / distance * push);
			}
			++hits;
		}
		return hits;
	}

	VillagerActor* Self(BT::Blackboard& board)
	{
		return static_cast<VillagerActor*>(board.self);
	}

	// 마을 한가운데서 leash 너머로 나가지 않게 목표를 당겨 온다.
	// 빛 밖으로 쫓아 나가면 어둠에 삼켜진다 — 마을 사람은 그걸 안다.
	void KeepInVillage(const BT::Blackboard& board, float& x, float& y)
	{
		const float dx = x - board.villageX;
		const float dy = y - board.villageY;
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance > board.leash && distance > 0.001f)
		{
			x = board.villageX + dx / distance * board.leash;
			y = board.villageY + dy / distance * board.leash;
		}
	}

	// 목표 쪽으로 한 걸음. 벽에 막혀 전혀 못 움직였으면 false.
	bool StepToward(BT::Blackboard& board, float targetX, float targetY, float speedScale)
	{
		Actor* self = board.self;
		float wantX = targetX - self->WorldX();
		float wantY = targetY - self->WorldY();
		const float length = std::sqrt(wantX * wantX + wantY * wantY);
		if (length < 0.5f)
		{
			return true;
		}
		wantX /= length;
		wantY /= length;

		const float step = std::min(length, board.walkSpeed * speedScale * board.context->deltaSeconds);
		float x = self->WorldX();
		float y = self->WorldY();
		const bool moved = board.context->game->MoveBody(x, y, wantX * step, wantY * step,
		                                                 kVillagerHalfSize, false);
		self->SetPosition(x, y);
		return moved;
	}

	// 돌을 던진다. 아직 손이 준비되지 않았으면 아무것도 안 한다.
	void ThrowAt(BT::Blackboard& board, float targetX, float targetY,
	             ThrownActor::Team team, float damage, int targetId)
	{
		if (board.attackCooldown > 0.0f)
		{
			return;
		}

		VillagerActor* self = Self(board);
		const float facing = (targetX >= self->WorldX()) ? 1.0f : -1.0f;
		std::unique_ptr<ThrownActor> stone(new ThrownActor(
			ThrownActor::Kind::Stone, team,
			self->WorldX() + facing * 8.0f, self->WorldY() - 20.0f,
			targetX, targetY, kStoneSpeed, damage, board.attackRange * 1.25f));
		stone->SetOwner(self->Id());
		stone->SetTarget(targetId);
		board.context->scene->Spawn(std::unique_ptr<Actor>(stone.release()));
		board.attackCooldown = board.attackEvery;
	}

	// 울타리 안쪽 안길 위의 한 점. 소란(외곽의 어둠)이 있으면 그쪽을 고른다.
	bool PickPatrolPoint(BT::Blackboard& board, float& outX, float& outY)
	{
		Game* game = board.context->game;

		float troubleAngle = 0.0f;
		bool hasTrouble = false;
		const std::vector<EnemyActor*>& enemies = game->Enemies();
		int seen = 0;
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			if (!enemies[i]->IsOutskirt() || enemies[i]->IsDying())
			{
				continue;
			}
			++seen;   // 여럿이면 하나를 고르게 고른다(저수지 표집)
			if (game->Roll(100 / seen))
			{
				troubleAngle = std::atan2(enemies[i]->WorldY() - board.villageY,
				                          enemies[i]->WorldX() - board.villageX);
				hasTrouble = true;
			}
		}

		for (int tries = 0; tries < 6; ++tries)
		{
			const float angle = hasTrouble ? troubleAngle + game->RandomRange(-0.35f, 0.35f)
			                               : game->RandomRange(0.0f, 6.2831853f);
			// 마을이 사각형이므로 방향을 사각형 둘레(안길)까지 늘린다
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			const float stretch = VillageLane() / std::max(std::fabs(c), std::fabs(s));
			const float x = board.villageX + c * stretch;
			const float y = board.villageY + s * stretch;
			if (!game->IsBodyBlocked(x, y, kVillagerHalfSize))
			{
				outX = x;
				outY = y;
				return true;
			}
		}
		return false;
	}

	// ── 조건 ──
	bool HoldsGrudge(BT::Blackboard& board)      { return board.grudge > 0.0f; }
	bool IsQuarreling(BT::Blackboard& board)     { return board.quarrel > 0.0f && board.quarry >= 0; }
	bool IsRunningOff(BT::Blackboard& board)     { return board.fleeing; }
	bool SeesEnemy(BT::Blackboard& board)        { return board.foe != NULL; }
	bool IsTraveling(BT::Blackboard& board)
	{
		const VillagerActor* self = Self(board);
		return self->Trip() >= 0 || self->HasPath();
	}
	bool PlayerIsNear(BT::Blackboard& board)     { return board.playerDistance < kNoticeRange; }
	bool IsInParty(BT::Blackboard& board)        { return Self(board)->IsHired(); }
	bool IsResting(BT::Blackboard& board)        { return board.timer > 0.0f; }
	bool HasSomewhereToGo(BT::Blackboard& board) { return board.hasTarget; }

	// ── 행동 ──

	// 원한을 품었다. 돌이 닿는 거리까지 다가가 플레이어에게 던진다.
	BT::Status TakeRevenge(BT::Blackboard& board)
	{
		board.activity = "복수한다";
		board.hasTarget = false;
		board.patrolling = false;
		board.timer = 0.0f;

		const PlayerActor* player = board.context->player;
		if (board.playerDistance > board.attackRange * 0.8f)
		{
			float x = player->WorldX();
			float y = player->WorldY();
			KeepInVillage(board, x, y);
			StepToward(board, x, y, 1.6f);   // 화가 나면 걸음이 빨라진다
		}
		if (board.playerDistance <= board.attackRange)
		{
			ThrowAt(board, player->WorldX(), player->WorldY() - 12.0f,
			        ThrownActor::Team::HitsPlayer, kStoneToPlayer, -1);
		}
		return BT::Status::Running;
	}

	// 마을 사람끼리 다툰다. 도둑을 쫓거나, 쫓기던 도둑이 돌아서서 맞선다.
	BT::Status Quarrel(BT::Blackboard& board)
	{
		VillagerActor* self = Self(board);
		VillagerActor* other = board.context->game->FindVillager(board.quarry);
		if (other == NULL || other->IsDead())
		{
			board.quarrel = 0.0f;
			board.quarry = -1;
			return BT::Status::Failure;
		}

		board.activity = (self->Stolen() > 0) ? "맞선다" : "도둑을 쫓는다";
		board.hasTarget = false;
		board.timer = 0.0f;

		const float distance = Distance(self->WorldX(), self->WorldY(), other->WorldX(), other->WorldY());
		if (distance > board.attackRange * 0.7f)
		{
			float x = other->WorldX();
			float y = other->WorldY();
			KeepInVillage(board, x, y);
			StepToward(board, x, y, 1.5f);
		}
		if (distance <= board.attackRange)
		{
			ThrowAt(board, other->WorldX(), other->BodyCenterY(),
			        ThrownActor::Team::HitsVillager, kStoneToVillager, other->Id());
		}
		return BT::Status::Running;
	}

	// 훔친 돈을 들고 집으로 달아난다. 집에 닿으면 그 돈은 제 것이 된다.
	BT::Status RunHome(BT::Blackboard& board)
	{
		board.activity = "달아난다";
		VillagerActor* self = Self(board);
		const float distance = Distance(self->WorldX(), self->WorldY(), board.homeX, board.homeY);
		if (distance <= kArriveRange * 2.0f)
		{
			board.fleeing = false;   // 집에 닿았다. 훔친 돈은 OnUpdate가 지갑에 넣는다
			return BT::Status::Success;
		}
		StepToward(board, board.homeX, board.homeY, 1.7f);
		return BT::Status::Running;
	}

	//
	// 길을 따라간다. 일행과 함께 이웃 마을로 가거나, 새 집 · 옛 집으로 걸어간다.
	// 길은 Game이 정해 준다(타일 너비 우선 탐색이라 집과 숲에 걸리지 않는다).
	//
	// 호위는 어둠이 다가오면 그 자리에 서서 맞선다. 길을 벗어나 쫓지는 않는다 —
	// 지켜야 할 사람이 뒤에 있다. 일행이 흩어지지 않게 Game이 정해 준 지점까지만
	// 앞서 간다.
	//
	BT::Status TravelWithTrip(BT::Blackboard& board)
	{
		VillagerActor* self = Self(board);
		board.hasTarget = false;
		board.timer = 0.0f;
		board.patrolling = false;

		if (self->Trip() >= 0 && self->IsEscort() && board.foe != NULL && board.foeDistance <= board.attackRange)
		{
			board.activity = "호위하며 싸운다";
			ThrowAt(board, board.foeX, board.foeY, ThrownActor::Team::HitsEnemies,
			        ProfessionOf(self->GetProfession()).stoneDamage, -1);
			return BT::Status::Running;
		}

		float x = 0.0f;
		float y = 0.0f;
		if (!self->Waypoint(x, y))
		{
			if (self->Trip() < 0)
			{
				self->ClearPath();   // 혼자 걷던 길이 끝났다
				return BT::Status::Success;
			}
			board.activity = self->PathDone() ? "일행을 기다린다" : "뒤처진 일행을 기다린다";
			return BT::Status::Running;
		}

		if (self->Trip() >= 0)
		{
			board.activity = self->IsEscort() ? "호위한다" : "길을 떠난다";
		}
		else
		{
			board.activity = "집으로 간다";
		}

		if (Distance(self->WorldX(), self->WorldY(), x, y) <= kArriveRange)
		{
			self->AdvanceWaypoint();
			return BT::Status::Running;
		}
		if (!StepToward(board, x, y, 1.5f))
		{
			// 길 위인데도 막혔다면 몸이 모서리에 걸린 것이다. 다음 점으로 넘어가 본다.
			self->AdvanceWaypoint();
		}
		return BT::Status::Running;
	}

	// 어둠이 보인다. 돌이 닿는 거리까지 다가가 던진다.
	BT::Status DefendVillage(BT::Blackboard& board)
	{
		VillagerActor* self = Self(board);
		board.activity = (self->GetProfession() == Profession::Hunter) ? "사냥한다" : "싸운다";
		board.hasTarget = false;
		board.timer = 0.0f;

		if (board.foeDistance > board.attackRange * 0.8f)
		{
			float x = board.foeX;
			float y = board.foeY;
			KeepInVillage(board, x, y);
			StepToward(board, x, y, 1.3f);
		}
		if (board.foeDistance <= board.attackRange)
		{
			ThrowAt(board, board.foeX, board.foeY, ThrownActor::Team::HitsEnemies,
			        ProfessionOf(self->GetProfession()).stoneDamage, -1);
		}
		return BT::Status::Running;
	}

	// 고용되어 플레이어와 함께 다닌다. 무엇을 할지는 갈래가 정한다.
	// 이름을 ServeParty로 하면 BuildTree(멤버 함수) 안에서 멤버 함수로 읽힌다.
	BT::Status TravelWithPlayer(BT::Blackboard& board)
	{
		Self(board)->ServeParty(*board.context);
		return BT::Status::Running;
	}

	// 하던 일을 멈추고 플레이어 쪽을 본다. 가까이 있는 동안 계속.
	BT::Status LookAtPlayer(BT::Blackboard& board)
	{
		board.activity = "쳐다본다";
		board.hasTarget = false;   // 가던 길은 잊는다. 떠나면 새로 고른다
		return BT::Status::Running;
	}

	const char* RestingActivity(const BT::Blackboard& board, const VillagerActor* self)
	{
		switch (static_cast<VillagerActor::Errand>(board.errand))
		{
		case VillagerActor::Errand::Patrol:   return "경계한다";
		case VillagerActor::Errand::Hunt:     return "사냥감을 기다린다";
		case VillagerActor::Errand::Decorate: return "꾸민다";
		case VillagerActor::Errand::Shop:     return "장을 본다";
		case VillagerActor::Errand::Doctor:   return "치료받는다";
		case VillagerActor::Errand::Tend:
			return ProfessionOf(self->GetProfession()).shop ? "장사한다" : "일한다";
		default:                              return "쉰다";
		}
	}

	const char* WalkingActivity(const BT::Blackboard& board)
	{
		switch (static_cast<VillagerActor::Errand>(board.errand))
		{
		case VillagerActor::Errand::Patrol:   return "순찰한다";
		case VillagerActor::Errand::Hunt:     return "사냥 나간다";
		case VillagerActor::Errand::Decorate: return "꾸미러 간다";
		case VillagerActor::Errand::Shop:     return "장 보러 간다";
		case VillagerActor::Errand::Doctor:   return "의사에게 간다";
		case VillagerActor::Errand::Steal:    return "슬금슬금 다가간다";
		case VillagerActor::Errand::Tend:     return "집으로 간다";
		default:                              return "걷는다";
		}
	}

	// 잠시 서 있는다. 무슨 일로 서 있는지는 심부름이 말해 준다.
	BT::Status Rest(BT::Blackboard& board)
	{
		board.activity = RestingActivity(board, Self(board));
		board.timer -= board.context->deltaSeconds;
		return (board.timer > 0.0f) ? BT::Status::Running : BT::Status::Success;
	}

	// 다음에 할 일을 고른다. 직업과 지갑과 몸 상태에 달려 있다.
	BT::Status ChooseErrand(BT::Blackboard& board)
	{
		Self(board)->PickErrand(*board.context);
		return BT::Status::Success;
	}

	// 목적지로 걷는다. 닿으면 그 일을 한다. 길이 막혔으면 목적지를 버리고 실패한다 —
	// 그러면 트리가 아래 가지로 내려가 다음 할 일을 고른다.
	BT::Status WalkToTarget(BT::Blackboard& board)
	{
		board.activity = WalkingActivity(board);

		Actor* self = board.self;
		const float distance = Distance(self->WorldX(), self->WorldY(), board.targetX, board.targetY);
		const bool steal = (static_cast<VillagerActor::Errand>(board.errand) == VillagerActor::Errand::Steal);

		if (distance <= (steal ? kStealReach : kArriveRange))
		{
			board.hasTarget = false;
			Self(board)->DoErrand(*board.context);
			return BT::Status::Success;
		}

		// 순찰과 사냥은 서둘러 간다. 도둑은 살금살금.
		float pace = 1.0f;
		const VillagerActor::Errand errand = static_cast<VillagerActor::Errand>(board.errand);
		if (errand == VillagerActor::Errand::Patrol || errand == VillagerActor::Errand::Hunt)
		{
			pace = 1.5f;
		}
		else if (steal)
		{
			pace = 0.8f;
		}

		if (!StepToward(board, board.targetX, board.targetY, pace))
		{
			board.hasTarget = false;   // 벽에 붙어 제자리걸음이다. 다른 일을 하자
			return BT::Status::Failure;
		}
		return BT::Status::Running;
	}
}

VillagerActor::VillagerActor(float worldX, float worldY, unsigned int look, Profession profession,
                             int perkIndex, Calling calling)
	: Actor("Villager")
	, m_Profession(profession)
	, m_Look(look)
	, m_PerkIndex(perkIndex)
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Object);

	m_Board.homeX = worldX;
	m_Board.homeY = worldY;
	m_Gold = ProfessionOf(profession).startGold;
	m_MaxHealth = ProfessionOf(profession).maxHealth;

	// 모험가는 갈래마다 몸이 다르다. 방패병은 단단하고 마법사는 여리다.
	if (profession == Profession::Adventurer)
	{
		const unsigned int kinds = static_cast<unsigned int>(Calling::Count) - 1u;
		m_Calling = (calling != Calling::None) ? calling
		                                       : static_cast<Calling>(1u + (look >> 5) % kinds);
		m_MaxHealth = CallingOf(m_Calling).maxHealth;
	}
	m_Health = m_MaxHealth;

	// 성향. 겉모습을 정한 값에서 서로 다른 자리를 떼어 쓴다.
	m_Greed      = static_cast<float>((look >> 3) % 101u) / 100.0f;
	m_Timidity   = static_cast<float>((look >> 11) % 101u) / 100.0f;
	m_Wanderlust = static_cast<float>((look >> 19) % 101u) / 100.0f;

	// 사람마다 조금씩 다르게. 가게 주인과 되찾아 온 사람은 자기 자리를 덜 벗어난다.
	const float spread = static_cast<float>(look % 90u);
	const bool settled = (perkIndex >= 0) || ProfessionOf(profession).shop;
	m_Board.wanderRadius = settled ? (90.0f + spread) : (170.0f + spread * 1.6f);
	m_Board.walkSpeed = 44.0f + static_cast<float>(look % 23u);
	m_Board.attackEvery = 1.5f + static_cast<float>(look % 5u) * 0.1f;

	switch (profession)
	{
	case Profession::Guard:
		m_Board.patrolChance = 65.0f;
		break;
	case Profession::Hunter:
		m_Board.patrolChance = 20.0f;
		m_Board.attackRange = 300.0f;   // 사냥꾼은 멀리 던진다
		m_Board.attackEvery = 1.2f;
		break;
	case Profession::Knight:
		m_Board.patrolChance = 40.0f;   // 일이 없으면 울타리를 돈다
		m_Board.attackEvery = 1.1f;
		break;
	case Profession::Adventurer:
		m_Board.patrolChance = 25.0f;
		m_Board.attackEvery = 1.3f;
		break;
	case Profession::Commoner:
		m_Board.patrolChance = (perkIndex >= 0) ? 5.0f : 15.0f;
		break;
	default:
		m_Board.patrolChance = 0.0f;   // 가게 주인은 가게를 비우지 않는다
		break;
	}

	BuildTree();
}

VillagerActor::~VillagerActor()
{
	// 목록에서 스스로 빠진다. 액터가 사라지는 길은 하나뿐이다.
	if (m_Game != NULL)
	{
		m_Game->UnregisterVillager(this);
	}
}

void VillagerActor::AttachToGame(Game* game, int village, float villageX, float villageY)
{
	m_Game = game;
	m_Id = game->NextVillagerId();
	m_Village = village;
	m_Board.villageX = villageX;
	m_Board.villageY = villageY;
	game->RegisterVillager(this);
}

void VillagerActor::Relocate(int village, float villageX, float villageY,
                             float homeX, float homeY, long long houseKey)
{
	m_Village = village;
	m_Board.villageX = villageX;
	m_Board.villageY = villageY;
	m_Board.homeX = homeX;
	m_Board.homeY = homeY;
	m_HouseKey = houseKey;
	m_Decorations = 0;   // 장식은 옛 집에 두고 왔다
	ClearWantsMove();
}

void VillagerActor::SetPath(const std::vector<std::pair<float, float> >& path)
{
	m_Path = path;
	m_PathIndex = 0;
	m_PathLimit = static_cast<size_t>(-1);
	m_Board.hasTarget = false;
	m_Board.timer = 0.0f;
}

bool VillagerActor::Waypoint(float& x, float& y) const
{
	if (m_PathIndex >= m_Path.size() || m_PathIndex >= m_PathLimit)
	{
		return false;
	}
	x = m_Path[m_PathIndex].first;
	y = m_Path[m_PathIndex].second;
	return true;
}

void VillagerActor::TakeMonsterHit(const ActorContext& context, float amount)
{
	if (m_Health <= 0.0f)
	{
		return;
	}
	amount *= CallingOf(m_Calling).toughness;   // 방패병은 덜 아프다
	m_Health -= amount;
	context.game->SpawnFloatingText(WorldX(), BodyCenterY(), "",
	                                static_cast<int>(amount + 0.5f), kColorHurt, 12.0f);
	if (m_Health <= 0.0f)
	{
		if (m_Hired)
		{
			context.game->SpawnFloatingText(WorldX(), WorldY() - 62.0f, DisplayName() + " 쓰러짐",
			                                -1, kColorHurt, 14.0f);
		}
		if (m_Stolen > 0)
		{
			Surrender(context);
		}
		Destroy();
	}
}

void VillagerActor::Wound(float amount)
{
	if (m_Health <= 0.0f)
	{
		return;
	}
	m_Health -= amount;
	if (m_Health <= 0.0f)
	{
		Destroy();
	}
}

std::string VillagerActor::DisplayName() const
{
	if (m_PerkIndex >= 0)
	{
		return Perks::At(m_PerkIndex).who;
	}
	if (m_Profession == Profession::Adventurer && m_Calling != Calling::None)
	{
		return std::string("모험가 · ") + CallingOf(m_Calling).name;
	}
	return ProfessionOf(m_Profession).name;
}

void VillagerActor::AddGold(int amount)
{
	m_Gold = std::max(0, m_Gold + amount);
}

int VillagerActor::TakeGold(int amount)
{
	const int taken = std::max(0, std::min(m_Gold, amount));
	m_Gold -= taken;
	return taken;
}

void VillagerActor::Provoke(float seconds)
{
	// 함께 다니는 사람은 둘레의 소란에 휩쓸려 등을 돌리지 않는다
	if (m_Health <= 0.0f || m_Hired)
	{
		return;
	}
	m_Board.grudge = std::max(m_Board.grudge, seconds);
	m_Board.hasTarget = false;   // 하던 일은 그만둔다
	m_Board.timer = 0.0f;
}

void VillagerActor::StartQuarrel(int quarryId, float seconds)
{
	if (m_Health <= 0.0f || quarryId == m_Id)
	{
		return;
	}
	m_Board.quarry = quarryId;
	m_Board.quarrel = std::max(m_Board.quarrel, seconds);
	m_Board.hasTarget = false;
	m_Board.timer = 0.0f;
}

void VillagerActor::EndQuarrel()
{
	m_Board.quarrel = 0.0f;
	m_Board.quarry = -1;
}

//
// 플레이어의 불씨에 맞았다. 체력이 0이 되면 사라진다.
// 그리고 원한을 품는다 — 맞은 사람만이 아니라 둘레의 사람들도 함께.
//
void VillagerActor::TakeDamage(const ActorContext& context, float amount)
{
	if (m_Health <= 0.0f)
	{
		return;
	}

	const bool fresh = !HasGrudge();
	m_Health -= amount;
	context.game->SpawnFloatingText(WorldX(), BodyCenterY(), "",
	                                static_cast<int>(amount + 0.5f), kColorHurt, 13.0f);

	Provoke(kGrudgeSeconds);
	context.game->RallyVillagers(WorldX(), WorldY(), kRallyRange, kGrudgeSeconds);

	if (fresh && m_Health > 0.0f)
	{
		context.game->SpawnFloatingText(WorldX(), WorldY() - 62.0f, "이놈!", -1, kAngerRed, 15.0f);
	}

	if (m_Health <= 0.0f)
	{
		// 들고 있던 훔친 돈은 주인에게 돌아간다
		if (m_Stolen > 0)
		{
			Surrender(context);
		}
		Destroy();
	}
}

//
// 마을 사람이 던진 돌에 맞았다. 아프기만 하고 죽지는 않는다.
// 훔친 돈을 들고 달아나던 도둑이라면, 한 대째에는 돌아서서 맞서고
// 두 대째에는 돈을 내놓는다.
//
void VillagerActor::TakeHitFrom(const ActorContext& context, float amount, int attackerId)
{
	if (m_Health <= 0.0f)
	{
		return;
	}

	m_Health = std::max(15.0f, m_Health - amount);
	context.game->SpawnFloatingText(WorldX(), BodyCenterY(), "",
	                                static_cast<int>(amount + 0.5f), kQuarrelOrange, 11.0f);

	if (m_Stolen > 0)
	{
		++m_HitsTaken;
		if (m_HitsTaken >= 2)
		{
			Surrender(context);
			return;
		}
		StartQuarrel(attackerId, 3.0f);   // 잠깐 돌아서서 맞선다. 끝나면 다시 달아난다
		context.game->SpawnFloatingText(WorldX(), WorldY() - 62.0f, "뭐야!", -1, kQuarrelOrange, 13.0f);
		return;
	}

	// 까닭 없이 맞으면 맞받아친다
	if (!InQuarrel())
	{
		StartQuarrel(attackerId, 4.0f);
	}
}

void VillagerActor::Surrender(const ActorContext& context)
{
	if (m_Stolen <= 0)
	{
		return;
	}

	if (VillagerActor* owner = context.game->FindVillager(m_StolenFrom))
	{
		owner->AddGold(m_Stolen);
	}
	m_Stolen = 0;
	m_StolenFrom = -1;
	m_HitsTaken = 0;
	m_Board.fleeing = false;
	EndQuarrel();
	context.game->EndQuarrelsAbout(m_Id);
	context.game->SpawnFloatingText(WorldX(), WorldY() - 62.0f, "잘못했소!", -1, kQuarrelOrange, 14.0f);
}

void VillagerActor::SetErrand(Errand errand, float targetX, float targetY, int with)
{
	m_Board.errand = static_cast<int>(errand);
	m_Board.errandWith = with;
	m_Board.targetX = targetX;
	m_Board.targetY = targetY;
	m_Board.hasTarget = true;
	m_Board.patrolling = (errand == Errand::Patrol || errand == Errand::Hunt);
}

//
// 다음에 할 일을 고른다. 위에서부터 먼저 해당하는 것을 한다.
//
//   1. 다쳤고 돈이 있으면 의사에게
//   2. 돈이 모였으면 집을 꾸민다
//   3. 가게 주인은 가게를 지킨다
//   4. 사냥꾼은 문으로 나가 어둠을 기다린다
//   5. 가난한 좀도둑은 부자를 노린다
//   6. 돈이 있으면 장을 본다
//   7. 순찰(경비는 자주)
//   8. 집에서 일하거나 둘레를 거닌다
//
void VillagerActor::PickErrand(const ActorContext& context)
{
	Game* game = context.game;
	const ProfessionInfo& info = ProfessionOf(m_Profession);
	const bool shopkeeper = info.shop && m_Profession != Profession::Hunter;
	const float awayFromHome = Distance(WorldX(), WorldY(), HomeX(), HomeY());

	m_Board.errandWith = -1;

	// 이사를 결심했다. 일행이 꾸려질 때까지 집 앞에서 짐을 싸며 기다린다.
	if (m_WantsMove >= 0)
	{
		if (awayFromHome > 30.0f)
		{
			SetErrand(Errand::Tend, HomeX(), HomeY());
		}
		else
		{
			m_Board.errand = static_cast<int>(Errand::Tend);
			m_Board.patrolling = false;
			m_Board.timer = game->RandomRange(3.0f, 6.0f);
		}
		return;
	}

	if (m_Health < MaxHealth() * 0.6f && m_Profession != Profession::Doctor
	    && m_Gold >= Prices::kVillagerDoctor)
	{
		if (VillagerActor* doctor = game->FindProfession(Profession::Doctor, m_Village))
		{
			SetErrand(Errand::Doctor, doctor->WorldX(), doctor->WorldY(), doctor->Id());
			return;
		}
	}

	if (m_Gold >= Prices::kDecor && m_Decorations < kMaxDecorations && game->Roll(shopkeeper ? 60 : 45))
	{
		SetErrand(Errand::Decorate, HomeX(), HomeY());
		return;
	}

	if (shopkeeper)
	{
		if (awayFromHome > 30.0f)
		{
			SetErrand(Errand::Tend, HomeX(), HomeY());
		}
		else
		{
			m_Board.errand = static_cast<int>(Errand::Tend);
			m_Board.patrolling = false;
			m_Board.timer = game->RandomRange(4.0f, 9.0f);
		}
		return;
	}

	if (m_Profession == Profession::Hunter && game->Roll(70))
	{
		float x = 0.0f;
		float y = 0.0f;
		if (PickPatrolPoint(m_Board, x, y))
		{
			SetErrand(Errand::Hunt, x, y);
			return;
		}
	}

	if (m_Thief && m_Gold < 12 && game->Roll(40))
	{
		if (VillagerActor* mark = game->RichestVillagerNear(WorldX(), WorldY(), 700.0f, m_Id, m_Village))
		{
			SetErrand(Errand::Steal, mark->WorldX(), mark->WorldY(), mark->Id());
			return;
		}
	}

	if (m_Profession != Profession::Merchant && m_Gold >= Prices::kVillagerShop + 2 && game->Roll(25))
	{
		if (VillagerActor* merchant = game->FindProfession(Profession::Merchant, m_Village))
		{
			SetErrand(Errand::Shop, merchant->WorldX(), merchant->WorldY(), merchant->Id());
			return;
		}
	}

	if (game->Roll(static_cast<int>(m_Board.patrolChance)))
	{
		float x = 0.0f;
		float y = 0.0f;
		if (PickPatrolPoint(m_Board, x, y))
		{
			SetErrand(Errand::Patrol, x, y);
			return;
		}
	}

	if (game->Roll(35))
	{
		if (awayFromHome > 40.0f)
		{
			SetErrand(Errand::Tend, HomeX(), HomeY());
		}
		else
		{
			m_Board.errand = static_cast<int>(Errand::Tend);
			m_Board.patrolling = false;
			m_Board.timer = game->RandomRange(3.0f, 6.0f);
		}
		return;
	}

	// 집 둘레를 거닌다
	for (int tries = 0; tries < 6; ++tries)
	{
		const float angle = game->RandomRange(0.0f, 6.2831853f);
		const float reach = game->RandomRange(m_Board.wanderRadius * 0.25f, m_Board.wanderRadius);
		const float x = HomeX() + std::cos(angle) * reach;
		const float y = HomeY() + std::sin(angle) * reach;
		if (!game->IsBodyBlocked(x, y, kVillagerHalfSize))
		{
			SetErrand(Errand::Stroll, x, y);
			return;
		}
	}

	m_Board.errand = static_cast<int>(Errand::None);
	m_Board.patrolling = false;
	m_Board.timer = 1.0f;   // 갈 곳을 못 찾았다. 잠시 쉬었다 다시 고른다
}

//
// 목적지에 닿았다. 그 일을 한다. 끝나면 잠시 서 있다가(Rest) 다음 일을 고른다.
//
void VillagerActor::DoErrand(const ActorContext& context)
{
	Game* game = context.game;

	switch (static_cast<Errand>(m_Board.errand))
	{
	case Errand::Patrol:
		m_Board.timer = game->RandomRange(3.0f, 6.0f);
		break;

	case Errand::Hunt:
		m_Board.timer = game->RandomRange(4.0f, 8.0f);
		break;

	case Errand::Tend:
		m_Board.timer = ProfessionOf(m_Profession).shop ? game->RandomRange(5.0f, 10.0f)
		                                                : game->RandomRange(3.0f, 6.0f);
		break;

	case Errand::Decorate:
		if (m_Gold >= Prices::kDecor && m_Decorations < kMaxDecorations)
		{
			m_Gold -= Prices::kDecor;
			game->PlaceDecoration(this, m_Decorations);
			++m_Decorations;
		}
		m_Board.timer = game->RandomRange(2.0f, 3.0f);
		break;

	case Errand::Shop:
		if (VillagerActor* merchant = game->FindVillager(m_Board.errandWith))
		{
			const int spent = TakeGold(Prices::kVillagerShop);
			merchant->AddGold(spent);
		}
		m_Board.timer = game->RandomRange(1.5f, 2.5f);
		break;

	case Errand::Doctor:
		if (VillagerActor* doctor = game->FindVillager(m_Board.errandWith))
		{
			doctor->AddGold(TakeGold(Prices::kVillagerDoctor));
			HealFully();
		}
		m_Board.timer = game->RandomRange(2.0f, 3.0f);
		break;

	case Errand::Steal:
		TrySteal(context);
		break;

	default:
		m_Board.timer = game->RandomRange(1.2f, 3.5f);
		break;
	}
}

void VillagerActor::TrySteal(const ActorContext& context)
{
	Game* game = context.game;
	VillagerActor* mark = game->FindVillager(m_Board.errandWith);
	if (mark == NULL || Distance(WorldX(), WorldY(), mark->WorldX(), mark->WorldY()) > kStealReach + 10.0f)
	{
		m_Board.timer = 1.0f;   // 놓쳤다
		return;
	}

	const int amount = mark->TakeGold(std::min(12, mark->Gold() / 3 + 2));
	if (amount <= 0)
	{
		m_Board.timer = 1.0f;
		return;
	}

	game->NoteTheft(m_Village);

	m_Stolen += amount;
	m_StolenFrom = mark->Id();
	m_HitsTaken = 0;
	m_Board.fleeing = true;
	m_Board.errand = static_cast<int>(Errand::None);

	// 들켰는가. 당한 사람이 알아채거나, 경비가 가까이 있으면 반드시 들킨다.
	const bool noticed = game->Roll(55) || game->GuardNear(WorldX(), WorldY(), kGuardNotice);
	if (noticed)
	{
		mark->StartQuarrel(m_Id, kQuarrelSeconds);
		game->RallyGuards(WorldX(), WorldY(), kGuardNotice + 60.0f, m_Id, kQuarrelSeconds);
		game->SpawnFloatingText(mark->WorldX(), mark->WorldY() - 62.0f, "도둑이야!", -1, kQuarrelOrange, 15.0f);
	}
}

// ═══ 고용된 모험가 ═════════════════════════════════════════════════
//
// 플레이어가 돈을 주고 고용하면 다음 마을까지 함께 다닌다. 줄 선 차례대로
// 플레이어의 발자취를 밟아 따라오고(동행자처럼), 적이 보이면 갈래대로 싸운다.
//
//   전사     달려들어 벤다. 칼끝 둘레의 적도 함께 맞고 밀려난다
//   방패병   플레이어와 적 사이에 막아선다. 이따금 둘레의 적을 제게로 끈다(도발)
//   궁수     걸으면서 쏜다. 적이 붙으면 물러나 거리를 벌린다
//   마법사   무리 한가운데 불덩이를 떨군다. 붙으면 물러난다
//   힐러     다친 사람(플레이어 · 파티)을 고친다. 한가하면 빛 조각을 던진다
//
// 플레이어에게서 너무 먼 적은 쫓지 않는다 — 지킬 사람을 두고 숲으로 들어가지 않는다.
// 피해는 플레이어 레벨에 따라 커진다(동행자와 같은 배율).
//
void VillagerActor::Hire(int untilVillage, int slot)
{
	m_Hired = true;
	m_HireUntil = untilVillage;
	m_PartySlot = slot;
	m_SkillTimer = 1.0f;

	// 하던 일은 모두 내려놓는다
	m_Board.hasTarget = false;
	m_Board.timer = 0.0f;
	m_Board.grudge = 0.0f;
	m_Board.patrolling = false;
	EndQuarrel();
	ClearWantsMove();
	ClearPath();
}

void VillagerActor::Dismiss()
{
	m_Hired = false;
	m_HireUntil = -1;
}

void VillagerActor::PartyStep(const ActorContext& context, float targetX, float targetY, float speed)
{
	float dx = targetX - WorldX();
	float dy = targetY - WorldY();
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.5f)
	{
		return;
	}
	const float step = std::min(length, speed * context.deltaSeconds);
	dx = dx / length * step;
	dy = dy / length * step;

	float x = WorldX();
	float y = WorldY();
	if (!context.game->MoveBody(x, y, dx, dy, kVillagerHalfSize, false))
	{
		// 모서리에 끼었다. 플레이어를 놓치는 것보다는 낫다 — 그냥 지나간다.
		x += dx;
		y += dy;
	}
	SetPosition(x, y);
}

void VillagerActor::FollowPlayer(const ActorContext& context)
{
	const PlayerActor* player = context.player;
	float x = player->WorldX();
	float y = player->WorldY();
	player->TrailPoint(kPartyLag + kPartySpacing * static_cast<float>(m_PartySlot), x, y);

	const float distance = Distance(WorldX(), WorldY(), x, y);
	if (distance < 6.0f)
	{
		return;
	}

	// 플레이어 걸음에 맞춘다. 많이 벌어지면 뛴다.
	float speed = Progression::MoveSpeed(player->Combat().level) * 1.05f;
	if (distance > 90.0f)
	{
		speed *= 1.7f;
	}
	PartyStep(context, x, y, speed);
}

void VillagerActor::ServeParty(const ActorContext& context)
{
	Game* game = context.game;
	PlayerActor* player = context.player;
	const CallingInfo& info = CallingOf(m_Calling);
	const float power = 1.0f + 0.12f * static_cast<float>(player->Combat().level - 1);

	m_Board.hasTarget = false;
	m_Board.timer = 0.0f;
	m_Board.patrolling = false;
	m_Board.activity = "따라간다";
	m_SkillTimer = std::max(0.0f, m_SkillTimer - context.deltaSeconds);

	if (game->IsRespawning())
	{
		m_Board.activity = "기다린다";
		return;
	}

	// 벽 너머에 걸렸거나 너무 떨어졌다. 화면 밖이니 플레이어의 발자취 위로 옮겨 붙는다.
	if (Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY()) > kPartyCatchUp)
	{
		float x = player->WorldX();
		float y = player->WorldY();
		player->TrailPoint(kPartyLag + kPartySpacing * static_cast<float>(m_PartySlot), x, y);
		SetPosition(x, y);
		return;
	}

	// ── 노릴 적 ── 내 둘레에서 가장 가까운 것. 플레이어에게서 먼 적은 두고 간다.
	EnemyActor* foe = NULL;
	float foeDistance = kPartyNotice;
	const std::vector<EnemyActor*>& enemies = game->Enemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor* enemy = enemies[i];
		if (enemy->IsDying()
		    || Distance(enemy->WorldX(), enemy->WorldY(), player->WorldX(), player->WorldY()) > kPartyLeash)
		{
			continue;
		}
		const float distance = Distance(WorldX(), BodyCenterY(), enemy->WorldX(), enemy->BodyCenterY());
		if (distance < foeDistance)
		{
			foeDistance = distance;
			foe = enemy;
		}
	}

	const float bodyX = WorldX() + m_Facing * 6.0f;
	const float bodyY = BodyCenterY();
	const bool ready = m_Board.attackCooldown <= 0.0f;
	const float run = Progression::MoveSpeed(player->Combat().level) * 1.15f;
	bool engaged = false;   // 싸우느라 줄을 벗어났는가. 아니면 따라가면서 싸운다

	auto face = [&](const Actor* target)
	{
		m_Facing = (target->WorldX() >= WorldX()) ? 1.0f : -1.0f;
		m_FacingHold = 0.5f;
	};

	// 적이 gap보다 가까우면 반대쪽으로 물러난다. 물러났으면 true.
	auto backOff = [&](const EnemyActor* from, float gap)
	{
		const float dx = WorldX() - from->WorldX();
		const float dy = WorldY() - from->WorldY();
		const float length = std::sqrt(dx * dx + dy * dy);
		if (length >= gap || length < 0.001f)
		{
			return false;
		}
		PartyStep(context, WorldX() + dx / length * 60.0f, WorldY() + dy / length * 60.0f, run);
		return true;
	};

	auto shoot = [&](ThrownActor::Kind kind, const EnemyActor* target, float speed, float range)
	{
		const bool lands = (kind == ThrownActor::Kind::Fireball);
		std::unique_ptr<ThrownActor> shot(new ThrownActor(
			kind, ThrownActor::Team::HitsEnemies, bodyX, bodyY,
			target->WorldX(), lands ? target->WorldY() : target->BodyCenterY(),
			speed, info.damage * power, range));
		shot->SetOwner(m_Id);   // 쓰러뜨리면 현상금은 이 사람 몫이다
		if (lands)
		{
			shot->SetBlast(78.0f);
		}
		context.scene->Spawn(std::unique_ptr<Actor>(shot.release()));
		m_Board.attackCooldown = info.every;
	};

	switch (m_Calling)
	{
	case Calling::Warrior:
		if (foe != NULL)
		{
			engaged = true;
			m_Board.activity = "벤다";
			face(foe);
			if (foeDistance > info.reach * 0.7f)
			{
				PartyStep(context, foe->WorldX(), foe->WorldY(), run);
			}
			if (ready && foeDistance <= info.reach + foe->Radius())
			{
				StrikeAround(context, foe->WorldX(), foe->BodyCenterY(), 40.0f, info.damage * power, 140.0f);
				std::unique_ptr<EffectActor> slash(new EffectActor(
					EffectActor::Kind::Slash, foe->WorldX(), foe->WorldY(), 30.0f, 0.22f, info.tint));
				slash->SetAngle(std::atan2(-(foe->WorldY() - WorldY()), foe->WorldX() - WorldX()));
				context.scene->Spawn(std::unique_ptr<Actor>(slash.release()));
				m_Board.attackCooldown = info.every;
			}
		}
		break;

	case Calling::Guardian:
		if (foe != NULL)
		{
			engaged = true;
			m_Board.activity = "막아선다";
			face(foe);

			// 적 바로 앞, 플레이어 쪽에 선다
			float standX = foe->WorldX();
			float standY = foe->WorldY();
			const float awayX = player->WorldX() - foe->WorldX();
			const float awayY = player->WorldY() - foe->WorldY();
			const float length = std::sqrt(awayX * awayX + awayY * awayY);
			if (length > 0.001f)
			{
				standX += awayX / length * 26.0f;
				standY += awayY / length * 26.0f;
			}
			if (Distance(WorldX(), WorldY(), standX, standY) > 8.0f)
			{
				PartyStep(context, standX, standY, run);
			}

			// 도발 — 둘레의 적이 한동안 나만 노린다
			if (m_SkillTimer <= 0.0f)
			{
				int drawn = 0;
				for (size_t i = 0; i < enemies.size(); ++i)
				{
					EnemyActor* enemy = enemies[i];
					if (!enemy->IsDying() && !enemy->IsWarden()
					    && Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->WorldY()) < 180.0f)
					{
						enemy->Taunt(m_Id, 4.5f);
						++drawn;
					}
				}
				if (drawn > 0)
				{
					context.scene->Spawn(std::unique_ptr<Actor>(new EffectActor(
						EffectActor::Kind::Shockwave, WorldX(), WorldY(), 180.0f, 0.45f, info.tint)));
					game->SpawnFloatingText(WorldX(), WorldY() - 62.0f, "도발", -1, info.tint, 13.0f);
					m_SkillTimer = 7.0f;
					m_CastGlow = 1.0f;
				}
			}

			if (ready && foeDistance <= info.reach + foe->Radius())
			{
				foe->TakeDamage(context, info.damage * power, bodyX, bodyY);
				m_Board.attackCooldown = info.every;
			}
		}
		break;

	case Calling::Archer:
		if (foe != NULL && foeDistance <= info.reach)
		{
			m_Board.activity = "쏜다";
			face(foe);
			engaged = backOff(foe, 130.0f);
			if (ready)
			{
				shoot(ThrownActor::Kind::Arrow, foe, 760.0f, info.reach * 1.2f);
			}
		}
		break;

	case Calling::Mage:
		if (foe != NULL && foeDistance <= info.reach)
		{
			m_Board.activity = "주문을 왼다";
			face(foe);
			engaged = backOff(foe, 110.0f);
			if (ready)
			{
				shoot(ThrownActor::Kind::Fireball, foe, 430.0f, 0.0f);
				m_CastGlow = 1.0f;
			}
		}
		break;

	case Calling::Healer:
	{
		// 가장 많이 다친 사람을 고친다. 플레이어는 75%, 파티는 60% 아래부터.
		if (m_SkillTimer <= 0.0f)
		{
			const Actor* patient = NULL;
			float worst = 1.0f;

			const float playerRatio = player->Combat().health / std::max(1.0f, player->MaxHealth());
			if (playerRatio < 0.75f && Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY()) < 320.0f)
			{
				patient = player;
				worst = playerRatio;
			}

			VillagerActor* ally = NULL;
			const std::vector<int>& party = game->Party();
			for (size_t i = 0; i < party.size(); ++i)
			{
				VillagerActor* member = game->FindVillager(party[i]);
				if (member == NULL || Distance(WorldX(), WorldY(), member->WorldX(), member->WorldY()) > 320.0f)
				{
					continue;
				}
				const float ratio = member->Health() / std::max(1.0f, member->MaxHealth());
				if (ratio < 0.6f && ratio < worst)
				{
					patient = member;
					ally = member;
					worst = ratio;
				}
			}

			if (patient != NULL)
			{
				if (ally != NULL)
				{
					ally->Heal(ally->MaxHealth() * 0.35f);
				}
				else
				{
					player->Heal(player->MaxHealth() * 0.15f);
				}
				std::unique_ptr<EffectActor> light(new EffectActor(
					EffectActor::Kind::Blessing, patient->WorldX(), patient->WorldY(), 40.0f, 0.9f, info.tint));
				light->Follow(patient);
				context.scene->Spawn(std::unique_ptr<Actor>(light.release()));
				game->SpawnFloatingText(patient->WorldX(), patient->WorldY() - 58.0f, "치유", -1, info.tint, 13.0f);
				m_Board.activity = "치유한다";
				m_SkillTimer = 5.0f;
				m_CastGlow = 1.0f;
			}
		}

		if (foe != NULL && foeDistance <= info.reach && ready)
		{
			face(foe);
			shoot(ThrownActor::Kind::Light, foe, 640.0f, info.reach * 1.2f);
		}
		break;
	}

	default:
		// 갈래가 없는 모험가(이론상 없다) — 돌을 던진다
		if (foe != NULL && foeDistance <= info.reach && ready)
		{
			face(foe);
			shoot(ThrownActor::Kind::Stone, foe, kStoneSpeed, info.reach * 1.25f);
		}
		break;
	}

	if (!engaged)
	{
		FollowPlayer(context);
	}
}

//
//   Selector
//    ├ [고용되었나?]           → 플레이어를 따라다니며 갈래대로 싸운다
//    ├ [원한이 있나?]          → 플레이어에게 돌을 던진다
//    ├ [다투는 중인가?]        → 도둑을 쫓는다 / 돌아서서 맞선다
//    ├ [훔쳐서 달아나는 중?]   → 집으로 달아난다
//    ├ [길을 가는 중인가?]     → 일행과 이웃 마을로 / 새 집으로. 호위는 어둠에 맞선다
//    ├ [어둠이 보이나?]        → 다가가 돌을 던진다
//    ├ [플레이어가 가까운가?]  → 멈춰서 쳐다본다
//    ├ [쉬는 중인가?]          → 장사한다 / 경계한다 / 쉰다 …
//    ├ [갈 곳이 있나?]         → 걷는다. 닿으면 그 일을 한다
//    └ [다음 할 일을 고른다]
//
// 위에 있는 가지가 늘 이긴다. 다툼이 달아나기보다 위에 있어서,
// 쫓기던 도둑이 한 대 맞으면 잠깐 돌아서서 맞섰다가 다시 달아난다.
//
void VillagerActor::BuildTree()
{
	BT::NodePtr root = BT::MakeSelector();
	BT::Selector* selector = static_cast<BT::Selector*>(root.get());

	struct Branch
	{
		BT::Condition::Test when;
		BT::Action::Run then;
	};
	const Branch branches[] =
	{
		{ IsInParty,        TravelWithPlayer },
		{ HoldsGrudge,      TakeRevenge },
		{ IsQuarreling,     Quarrel },
		{ IsRunningOff,     RunHome },
		{ IsTraveling,      TravelWithTrip },
		{ SeesEnemy,        DefendVillage },
		{ PlayerIsNear,     LookAtPlayer },
		{ IsResting,        Rest },
		{ HasSomewhereToGo, WalkToTarget },
	};

	for (size_t i = 0; i < sizeof(branches) / sizeof(branches[0]); ++i)
	{
		BT::NodePtr branch = BT::MakeSequence();
		BT::Sequence* sequence = static_cast<BT::Sequence*>(branch.get());
		sequence->Add(BT::MakeCondition(branches[i].when));
		sequence->Add(BT::MakeAction(branches[i].then));
		selector->Add(std::move(branch));
	}
	selector->Add(BT::MakeAction(ChooseErrand));

	m_Tree = std::move(root);
}

// 장보기 · 치료 · 도둑질은 상대가 움직이므로 목적지를 매번 고친다.
void VillagerActor::FollowErrandTarget(const ActorContext& context)
{
	const Errand errand = static_cast<Errand>(m_Board.errand);
	if (!m_Board.hasTarget || m_Board.errandWith < 0
	    || (errand != Errand::Shop && errand != Errand::Doctor && errand != Errand::Steal))
	{
		return;
	}

	VillagerActor* other = context.game->FindVillager(m_Board.errandWith);
	if (other == NULL || other->IsDead())
	{
		m_Board.hasTarget = false;
		m_Board.errand = static_cast<int>(Errand::None);
		return;
	}

	// 손님은 상대 옆에 선다. 도둑은 바짝 붙는다.
	const float side = (errand == Errand::Steal) ? 0.0f : ((WorldX() < other->WorldX()) ? -18.0f : 18.0f);
	m_Board.targetX = other->WorldX() + side;
	m_Board.targetY = other->WorldY();
}

void VillagerActor::OnUpdate(const ActorContext& context)
{
	const float deltaSeconds = context.deltaSeconds;
	const float beforeX = WorldX();
	const float beforeY = WorldY();

	m_Board.attackCooldown = std::max(0.0f, m_Board.attackCooldown - deltaSeconds);
	m_CastGlow = std::max(0.0f, m_CastGlow - deltaSeconds * 2.5f);
	m_FacingHold = std::max(0.0f, m_FacingHold - deltaSeconds);
	m_Board.grudge = std::max(0.0f, m_Board.grudge - deltaSeconds);
	m_Board.quarrel = std::max(0.0f, m_Board.quarrel - deltaSeconds);
	if (m_Board.quarrel <= 0.0f)
	{
		m_Board.quarry = -1;
	}

	// 쓰러뜨렸으면 됐다. 다시 일어난 사람에게까지 원한을 품지는 않는다.
	if (context.game->IsRespawning())
	{
		m_Board.grudge = 0.0f;
	}

	// 천천히 아문다
	m_Health = std::min(MaxHealth(), m_Health + deltaSeconds / 3.0f);

	// 이사하고 싶은 마음은 일행이 꾸려지지 않으면 시간이 지나며 식는다
	if (m_WantsMove >= 0 && m_Trip < 0)
	{
		m_WantTimer -= deltaSeconds;
		if (m_WantTimer <= 0.0f)
		{
			ClearWantsMove();
		}
	}

	// 품삯. 사냥꾼은 현상금으로, 가게 주인은 손님에게서 번다.
	m_WageTimer -= deltaSeconds;
	if (m_WageTimer <= 0.0f)
	{
		m_WageTimer = context.game->RandomRange(12.0f, 18.0f);
		if (m_Profession == Profession::Commoner || m_Profession == Profession::Guard)
		{
			AddGold(2);
		}
		else if (m_Profession == Profession::Informant)
		{
			AddGold(1);
		}
	}

	// 훔친 돈을 들고 집에 닿았으면 제 것이 된다
	if (m_Stolen > 0 && !m_Board.fleeing)
	{
		m_Gold += m_Stolen;
		m_Stolen = 0;
		m_StolenFrom = -1;
	}

	// 빛이 닿는 곳까지가 마을이다. 종지기 노인이 오면 그만큼 넓어진다.
	// (길을 가는 동안은 이 줄에 매이지 않는다 — 여정 가지가 따로 움직인다)
	m_Board.leash = BeaconRadius() * context.game->Mods().beaconScale - 50.0f;

	// 칠판에 이번 프레임의 사정을 적어 둔다. 마디들은 이것만 보고 판단한다.
	m_Board.self = this;
	m_Board.context = &context;
	m_Board.playerDistance = Distance(WorldX(), WorldY(),
	                                  context.player->WorldX(), context.player->WorldY());

	FollowErrandTarget(context);

	// 노릴 어둠을 찾는다. 가장 가까운 것 하나. 도둑질 중이거나 달아나는 중에는 못 본 척한다.
	m_Board.foe = NULL;
	m_Board.foeDistance = m_Board.noticeRange;
	if (!m_Board.fleeing && static_cast<Errand>(m_Board.errand) != Errand::Steal)
	{
		const std::vector<EnemyActor*>& enemies = context.game->Enemies();
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			EnemyActor* enemy = enemies[i];
			if (enemy->IsDying())
			{
				continue;
			}
			const float distance = Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->BodyCenterY());
			if (distance < m_Board.foeDistance)
			{
				m_Board.foe = enemy;
				m_Board.foeX = enemy->WorldX();
				m_Board.foeY = enemy->BodyCenterY();
				m_Board.foeDistance = distance;
			}
		}
	}

	m_Tree->Tick(m_Board);

	// 몸을 돌린다. 다투거나 싸우는 중이면 상대 쪽을, 아니면 걸어가는 쪽을 본다.
	const float moved = WorldX() - beforeX;
	m_Walking = std::fabs(moved) > 0.001f || std::fabs(WorldY() - beforeY) > 0.001f || m_Board.hasTarget;

	VillagerActor* quarry = InQuarrel() ? context.game->FindVillager(m_Board.quarry) : NULL;
	if (m_Hired)
	{
		// 함께 다니는 사람은 싸운 쪽을 잠깐 보고, 아니면 걸어가는 쪽을 본다(ServeParty)
		if (m_FacingHold <= 0.0f && std::fabs(moved) > 0.001f)
		{
			m_Facing = (moved > 0.0f) ? 1.0f : -1.0f;
		}
	}
	else if (HasGrudge() || (m_Board.playerDistance < kNoticeRange && quarry == NULL))
	{
		m_Facing = (context.player->WorldX() >= WorldX()) ? 1.0f : -1.0f;
	}
	else if (quarry != NULL)
	{
		m_Facing = (quarry->WorldX() >= WorldX()) ? 1.0f : -1.0f;
	}
	else if (m_Board.foe != NULL)
	{
		m_Facing = (m_Board.foeX >= WorldX()) ? 1.0f : -1.0f;
	}
	else if (std::fabs(moved) > 0.001f)
	{
		m_Facing = (moved > 0.0f) ? 1.0f : -1.0f;
	}

	m_Bob += deltaSeconds * (m_Walking ? 7.0f : 1.4f);
}

void VillagerActor::OnDraw(const DrawContext& context) const
{
	Renderer* r = context.renderer;
	const float x = context.ToRenderX(WorldX());
	const float y = context.ToRenderY(WorldY());
	const bool angry = HasGrudge();
	const bool quarreling = InQuarrel();
	const ProfessionInfo& info = ProfessionOf(m_Profession);

	// 첫 마을은 늘 밝지만, 들고 있는 등불이 가까우면 조금 더 또렷해진다.
	const float shade = std::min(1.0f, 0.62f
		+ context.game->TotalBrightness(WorldX(), WorldY()) * 0.45f);

	// 원한(플레이어에게)은 붉게, 다툼(마을 사람끼리)은 주황으로 발밑이 일렁인다
	if (angry || quarreling)
	{
		const float* hue = angry ? kAngerRed : kQuarrelOrange;
		const float pulse = 0.6f + 0.4f * std::sin(context.time * 7.0f);
		const Color aura = { hue[0], hue[1], hue[2], 0.38f * pulse };
		r->DrawEllipse(x, y, 16.0f, 6.0f, aura, 0.0f, 0.9f * pulse, 3.0f);
	}

	DrawShadow(r, x, y, 11.0f);

	// 모험가는 갈래의 빛깔을 입는다
	const bool adventurer = (m_Profession == Profession::Adventurer && m_Calling != Calling::None);
	const CallingInfo& calling = CallingOf(m_Calling);

	// 고용된 사람은 발밑에 제 빛깔의 고리가 돈다. 파티가 누구인지 한눈에 보이도록.
	if (m_Hired)
	{
		const Color ring = { calling.tint[0], calling.tint[1], calling.tint[2], 0.55f };
		r->DrawEllipse(x, y, 15.0f, 5.5f, ring, 0.0f, 0.5f + 0.6f * m_CastGlow, 1.5f);
	}

	FigureLook look;
	look.body = Tint(adventurer ? calling.tint : info.tint, shade);
	look.skin = Tint(kLiving, shade);
	look.hair = Tint(kHair, shade);
	look.hooded = (m_Profession == Profession::Hunter || m_Profession == Profession::Informant)
	           || (m_Profession == Profession::Commoner && (m_Look % 3u) == 0u);
	look.posture = 1.0f;
	look.facing = m_Facing;
	look.bob = m_Walking ? std::sin(m_Bob) * 1.6f : std::sin(m_Bob) * 0.4f;

	if (m_Profession == Profession::Guard)
	{
		look.hair = Tint(kSteel, shade * 0.9f);   // 투구
	}

	// 되찾아 온 사람은 제 일의 빛깔을 걸친다. 누가 돌아왔는지 한눈에 보이도록.
	if (m_PerkIndex >= 0)
	{
		const float* cloth = (m_PerkIndex % 2 == 0) ? kCloak : kHood;
		look.body = Tint(cloth, shade * 1.1f);
		look.hooded = true;
	}

	// 원한을 품으면 옷빛이 붉게 물든다. 누가 화가 났는지 멀리서도 보이도록.
	if (angry)
	{
		look.body = Color{ Mix(look.body.r, kAngerRed[0] * shade, 0.55f),
		                   Mix(look.body.g, kAngerRed[1] * shade, 0.55f),
		                   Mix(look.body.b, kAngerRed[2] * shade, 0.55f), 1.0f };
	}

	if (m_Profession == Profession::Knight
	    || (adventurer && (m_Calling == Calling::Guardian || m_Calling == Calling::Warrior)))
	{
		look.hair = Tint(kSteel, shade * 1.05f);   // 투구
	}
	if (adventurer && m_Calling == Calling::Healer)
	{
		look.hooded = true;
		look.hair = Tint(kStone, shade * 2.1f);    // 흰 두건
	}

	// 이사 가는 사람은 등에 짐을 진다
	const bool moving = (m_Trip >= 0 && !m_Escort && m_Profession != Profession::Courier);
	if (moving)
	{
		r->DrawRoundRect(x - m_Facing * 7.0f, y + 22.0f, 11.0f, 13.0f, 3.0f, Tint(kWood, shade));
	}

	// 궁수는 등에 화살통을 멘다(몸보다 먼저 그려 뒤에 보이게)
	if (adventurer && m_Calling == Calling::Archer)
	{
		r->DrawRoundRect(x - m_Facing * 6.0f, y + 20.0f, 5.0f, 13.0f, 2.0f, Tint(kWoodDark, shade * 1.2f),
		                 m_Facing * 0.35f);
		r->DrawSegment(x - m_Facing * 7.5f, y + 26.0f, x - m_Facing * 9.5f, y + 31.0f, 1.2f, Tint(kHair, shade * 1.4f));
		r->DrawSegment(x - m_Facing * 5.0f, y + 26.5f, x - m_Facing * 6.0f, y + 32.0f, 1.2f, Tint(kHair, shade * 1.4f));
	}

	float handX = x;
	float handY = y;
	DrawFigure(r, x, y, look, &handX, &handY);

	if (adventurer)
	{
		const float f = m_Facing;
		switch (m_Calling)
		{
		case Calling::Warrior:
			// 손에 든 긴 칼
			r->DrawSegment(handX, handY, handX + f * 6.0f, handY + 20.0f, 2.6f, Tint(kSteel, shade * 1.25f));
			r->DrawSegment(handX - f * 3.0f, handY + 3.0f, handX + f * 3.5f, handY + 1.5f, 2.0f, Tint(kGold, shade));
			break;

		case Calling::Guardian:
			// 앞에 든 큰 방패
			r->DrawRoundRect(x + f * 9.0f, y + 15.0f, 10.0f, 16.0f, 3.0f, Tint(kSteel, shade * 0.95f));
			r->DrawRoundRect(x + f * 9.0f, y + 15.0f, 6.0f, 12.0f, 2.0f, Tint(calling.tint, shade * 1.1f));
			r->DrawCircle(x + f * 9.0f, y + 15.5f, 1.6f, Tint(kGold, shade));
			break;

		case Calling::Archer:
		{
			// 손에 쥔 활 — 앞으로 휜 활대와 곧은 시위
			const float bowX = handX + f * 2.0f;
			const float bowY = handY + 6.0f;
			float prevX = bowX;
			float prevY = bowY - 11.0f;
			for (int i = 1; i <= 6; ++i)
			{
				const float t = static_cast<float>(i) / 6.0f;
				const float px = bowX + f * std::sin(t * 3.14159265f) * 5.5f;
				const float py = bowY - 11.0f + 22.0f * t;
				r->DrawSegment(prevX, prevY, px, py, 1.8f, Tint(kWood, shade * 1.2f));
				prevX = px;
				prevY = py;
			}
			r->DrawSegment(bowX, bowY - 11.0f, bowX, bowY + 11.0f, 0.8f, Tint(kStone, shade * 2.0f));
			break;
		}

		case Calling::Healer:
		{
			// 끝이 빛나는 지팡이
			const float topY = handY + 22.0f;
			r->DrawSegment(handX, handY - 9.0f, handX, topY, 1.8f, Tint(kWood, shade * 1.2f));
			const float glow = 0.6f + 0.4f * std::sin(context.time * 3.0f) + m_CastGlow;
			r->DrawCircle(handX, topY + 2.0f, 3.2f, Tint(calling.tint, 1.15f), 0.8f * glow);
			break;
		}

		case Calling::Mage:
		{
			// 뾰족 모자와 보랏빛 구슬 지팡이
			r->DrawTriangle(x - f * 0.5f, y + 37.0f + look.bob, 15.0f, 14.0f, Tint(calling.tint, shade * 0.8f), -f * 0.18f);
			r->DrawRoundRect(x, y + 31.0f + look.bob, 16.0f, 2.6f, 1.2f, Tint(calling.tint, shade * 0.7f));
			const float topY = handY + 20.0f;
			r->DrawSegment(handX, handY - 8.0f, handX, topY, 1.8f, Tint(kWoodDark, shade * 1.3f));
			const float pulse = 0.6f + 0.4f * std::sin(context.time * 4.0f) + m_CastGlow;
			const float orb[3] = { 0.78f, 0.56f, 1.0f };
			r->DrawCircle(handX, topY + 2.5f, 3.4f, Tint(orb, 1.0f), 1.0f * pulse);
			break;
		}

		default:
			break;
		}
	}

	// 기사(와 갈래 없는 모험가)는 칼을 찬다, 전령은 편지 가방을 멘다
	if (m_Profession == Profession::Knight || (m_Profession == Profession::Adventurer && !adventurer))
	{
		r->DrawSegment(x + m_Facing * 7.0f, y + 6.0f, x + m_Facing * 9.0f, y + 22.0f, 2.2f,
		               Tint(kSteel, shade * 1.2f));
	}
	else if (m_Profession == Profession::Courier)
	{
		r->DrawRoundRect(x - m_Facing * 5.0f, y + 12.0f, 9.0f, 7.0f, 1.5f, Tint(kWood, shade * 1.15f));
		r->DrawRoundRect(x - m_Facing * 5.0f, y + 12.5f, 5.0f, 2.0f, 0.5f, Tint(kStone, shade * 2.0f));
	}

	const float bob = std::sin(context.time * 6.0f) * 1.5f;

	// 고용된 사람은 머리 위에 체력이 보인다. 지켜 줘야 할 사람이기도 하다.
	if (m_Hired)
	{
		const float barY = y + 50.0f;
		const float ratio = std::max(0.0f, m_Health / std::max(1.0f, m_MaxHealth));
		const Color track = { 0.03f, 0.03f, 0.05f, 0.8f };
		const Color fill = (ratio < 0.35f) ? Color{ 0.85f, 0.25f, 0.2f, 1.0f }
		                                   : Color{ calling.tint[0], calling.tint[1], calling.tint[2], 1.0f };
		r->DrawRoundRect(x, barY, 28.0f, 5.0f, 2.5f, track);
		r->DrawRoundRect(x - 13.0f * (1.0f - ratio), barY, 26.0f * ratio, 3.0f, 1.5f, fill);
	}

	// 머리 위의 느낌표 — 붉으면 플레이어에게, 주황이면 마을 사람에게 화가 났다
	if (angry || quarreling)
	{
		const float fade = angry ? std::min(1.0f, m_Board.grudge / 2.0f)
		                         : std::min(1.0f, m_Board.quarrel / 1.5f);
		const float* hue = angry ? kAngerRed : kQuarrelOrange;
		const float markY = y + 50.0f + bob;
		const Color backing = { 0.05f, 0.03f, 0.03f, 0.7f * fade };
		const Color mark = { hue[0], hue[1], hue[2], fade };

		r->DrawCircle(x, markY, 8.5f, backing);
		r->DrawRoundRect(x, markY + 1.6f, 3.0f, 8.0f, 1.4f, mark, 0.0f, 1.4f * fade);
		r->DrawCircle(x, markY - 5.0f, 1.7f, mark, 1.4f * fade);
	}
	// 훔친 돈주머니를 들고 있으면 머리 위에 보인다 — 누가 훔쳤는지 플레이어는 안다
	else if (m_Stolen > 0)
	{
		const float markY = y + 48.0f + bob;
		r->DrawCircle(x, markY, 6.5f, Tint(kWood, shade * 1.1f));
		r->DrawRoundRect(x, markY + 6.0f, 5.0f, 3.0f, 1.0f, Tint(kWoodDark, shade));
		r->DrawCircle(x + 1.5f, markY - 1.0f, 2.2f, Tint(kGold), 0.8f);
	}
}
