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
			if (villagers[i]->IsDead())
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

bool PlayerActor::UseTorch()
{
	if (m_Combat.torches <= 0)
	{
		return false;
	}
	--m_Combat.torches;
	return true;
}

bool PlayerActor::UseHealPack()
{
	// 멀쩡할 때는 아깝게 마시지 않는다
	if (m_Combat.healPacks <= 0 || m_Combat.health >= MaxHealth())
	{
		return false;
	}
	--m_Combat.healPacks;
	Heal(MaxHealth() * 0.35f);
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
	const float kStoneDamage = 9.0f;       // 적에게 던지는 돌. 떠도는 그림자를 세 번에 쫓는다
	const float kHunterStoneDamage = 14.0f;// 사냥꾼은 솜씨가 좋다
	const float kStoneToPlayer = 4.0f;     // 원한을 품고 던지는 돌
	const float kStoneToVillager = 3.0f;   // 마을 사람끼리 다툴 때 — 아프기만 하다
	const float kStoneSpeed = 360.0f;
	const float kQuarrelSeconds = 12.0f;   // 도둑을 쫓는 시간
	const float kStealReach = 26.0f;       // 이만큼 붙어야 주머니를 턴다
	const float kGuardNotice = 260.0f;     // 경비가 이 안에 있으면 도둑질은 반드시 들킨다
	const float kAngerRed[3] = { 0.92f, 0.20f, 0.15f };
	const float kQuarrelOrange[3] = { 1.00f, 0.62f, 0.20f };

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
	bool PlayerIsNear(BT::Blackboard& board)     { return board.playerDistance < kNoticeRange; }
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
			const float damage = (self->GetProfession() == Profession::Hunter) ? kHunterStoneDamage : kStoneDamage;
			ThrowAt(board, board.foeX, board.foeY, ThrownActor::Team::HitsEnemies, damage, -1);
		}
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
                             int perkIndex)
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

void VillagerActor::AttachToGame(Game* game, float villageX, float villageY)
{
	m_Game = game;
	m_Id = game->NextVillagerId();
	m_Board.villageX = villageX;
	m_Board.villageY = villageY;
	game->RegisterVillager(this);
}

std::string VillagerActor::DisplayName() const
{
	if (m_PerkIndex >= 0)
	{
		return Perks::At(m_PerkIndex).who;
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
	if (m_Health <= 0.0f)
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

	if (m_Health < MaxHealth() * 0.6f && m_Profession != Profession::Doctor
	    && m_Gold >= Prices::kVillagerDoctor)
	{
		if (VillagerActor* doctor = game->FindProfession(Profession::Doctor))
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
		if (VillagerActor* mark = game->RichestVillagerNear(WorldX(), WorldY(), 700.0f, m_Id))
		{
			SetErrand(Errand::Steal, mark->WorldX(), mark->WorldY(), mark->Id());
			return;
		}
	}

	if (m_Profession != Profession::Merchant && m_Gold >= Prices::kVillagerShop + 2 && game->Roll(25))
	{
		if (VillagerActor* merchant = game->FindProfession(Profession::Merchant))
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

//
//   Selector
//    ├ [원한이 있나?]          → 플레이어에게 돌을 던진다
//    ├ [다투는 중인가?]        → 도둑을 쫓는다 / 돌아서서 맞선다
//    ├ [훔쳐서 달아나는 중?]   → 집으로 달아난다
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
		{ HoldsGrudge,      TakeRevenge },
		{ IsQuarreling,     Quarrel },
		{ IsRunningOff,     RunHome },
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

	m_Board.attackCooldown = std::max(0.0f, m_Board.attackCooldown - deltaSeconds);
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
	m_Walking = std::fabs(moved) > 0.001f || m_Board.hasTarget;

	VillagerActor* quarry = InQuarrel() ? context.game->FindVillager(m_Board.quarry) : NULL;
	if (HasGrudge() || (m_Board.playerDistance < kNoticeRange && quarry == NULL))
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

	FigureLook look;
	look.body = Tint(info.tint, shade);
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

	DrawFigure(r, x, y, look);

	const float bob = std::sin(context.time * 6.0f) * 1.5f;

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
