#include "stdafx.h"
#include "Actors.h"

#include "Art.h"
#include "Game.h"
#include "Input.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"

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
	return Progression::MaxHealth(m_Combat.level);
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
	const int tier = m_Combat.weaponTier;
	const float range = Progression::AttackRange(level);

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

	const int shots = Progression::ProjectileCount(level);
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

	m_Combat.invulnerable = std::max(0.0f, m_Combat.invulnerable - deltaSeconds);
	m_Combat.hurtFlash = std::max(0.0f, m_Combat.hurtFlash - deltaSeconds * 2.5f);
	m_Combat.magnetTime = std::max(0.0f, m_Combat.magnetTime - deltaSeconds);
	m_Combat.levelUpGlow = std::max(0.0f, m_Combat.levelUpGlow - deltaSeconds * 0.8f);
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
	m_Combat.invulnerable = kInvulnerableTime;
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
	m_Combat.souls += amount;

	while (m_Combat.souls >= Progression::SoulsToNextLevel(m_Combat.level))
	{
		m_Combat.souls -= Progression::SoulsToNextLevel(m_Combat.level);

		const float previousMax = Progression::MaxHealth(m_Combat.level);
		++m_Combat.level;
		const float newMax = Progression::MaxHealth(m_Combat.level);

		// 늘어난 최대 체력만큼 채우고, 거기에 조금 더 회복한다
		m_Combat.health = std::min(newMax, m_Combat.health + (newMax - previousMax)
		                                   + newMax * Progression::LevelUpHealRatio());
		m_Combat.levelUpGlow = 1.0f;
		context.game->SpawnFloatingText(WorldX(), WorldY() - 60.0f, "레벨 업!", -1, kColorGold, 20.0f);
	}
}

void PlayerActor::Heal(float amount)
{
	m_Combat.health = std::min(MaxHealth(), m_Combat.health + amount);
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
CompanionActor::CompanionActor(float worldX, float worldY, int regionX, int regionY)
	: Actor("Companion")
	, m_RegionX(regionX)
	, m_RegionY(regionY)
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

	// 완전히 굳으면 그 자리에 멈춘다. 죽는 것이 아니라, 빛을 되돌려 비추면 깨어난다.
	if (IsFrozenAgain())
	{
		m_Walking = false;
		return;
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
	const float facing = (context.game->PlayerX() >= WorldX()) ? 1.0f : -1.0f;
	DrawCompanion(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	              1.0f - m_Darkness, facing, m_Walking, context.time);
}
