#include "stdafx.h"
#include "Actors.h"

#include "Art.h"
#include "Game.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>

using namespace Art;

namespace
{
	const float kTwoPi = 6.2831853f;
	const float kPlayerRadius = 10.0f;

	const float kDespawnDistance = 1500.0f;
	const float kLightSlow = 0.6f;         // 불빛 안에서는 발이 무거워진다

	// ── 종탑지기 ──
	const float kWardenAggro = 260.0f;     // 종탑에 이만큼 다가오면 덤벼든다
	const float kWardenLeash = 650.0f;     // 이만큼 멀어지면 제자리로 돌아가 상처를 회복한다
	const float kWardenBurstEvery = 3.0f;
	const int   kWardenOrbs = 12;
	const float kOrbSpeed = 175.0f;
	const float kOrbLife = 3.2f;

	// ── 떨어진 것 ──
	const float kMagnetSeconds = 10.0f;
	const float kMagnetRadius = 900.0f;
	const float kPickupReach = 18.0f;
	const float kPickupLifetime = 90.0f;
	const float kPickupSettle = 0.35f;     // 흩어진 뒤 잠깐은 끌려오지 않는다

	const float kColorDamage[3] = { 1.00f, 0.86f, 0.62f };
	const float kColorSoul[3]   = { 0.62f, 0.82f, 1.00f };
	const float kColorHeal[3]   = { 0.50f, 0.95f, 0.58f };
	const float kColorGold[3]   = { 1.00f, 0.82f, 0.40f };
}

// ═══ 적 ═══════════════════════════════════════════════════════════
EnemyActor::EnemyActor(EnemyKind kind, float worldX, float worldY, float power)
	: Actor("Enemy")
{
	SetPosition(worldX, worldY);

	const EnemyType& type = EnemyTypeOf(kind);
	m_Data.kind = kind;
	m_Data.power = power;
	m_Data.maxHealth = type.health * power;
	m_Data.health = m_Data.maxHealth;
	m_Data.touchTimer = 0.5f;
}

EnemyActor::~EnemyActor()
{
	// 목록에서 스스로 빠진다. 액터가 사라지는 길은 하나뿐이다.
	if (m_Game != NULL)
	{
		m_Game->UnregisterEnemy(this);
	}
}

void EnemyActor::AttachToGame(Game* game)
{
	m_Game = game;
	m_Data.id = game->NextEnemyId();
	game->RegisterEnemy(this);
}

void EnemyActor::MakeWarden(int regionX, int regionY, float homeX, float homeY)
{
	m_Data.regionX = regionX;
	m_Data.regionY = regionY;
	m_Data.homeX = homeX;
	m_Data.homeY = homeY;
	m_Data.burstTimer = kWardenBurstEvery;
}

float EnemyActor::Radius() const
{
	return EnemyTypeOf(m_Data.kind).radius;
}

float EnemyActor::BodyCenterY() const
{
	// 발밑에서 몸 가운데까지. 월드는 y가 아래로 커지므로 위쪽은 빼기다.
	return WorldY() - Radius();
}

void EnemyActor::OnUpdate(const ActorContext& context)
{
	const float deltaSeconds = context.deltaSeconds;

	// 흩어지는 중이면 사라질 때까지 세기만 한다.
	if (m_Data.dying > 0.0f)
	{
		m_Data.dying += deltaSeconds;
		if (m_Data.dying > 0.45f)
		{
			Destroy();
		}
		return;
	}

	PlayerActor* player = context.player;
	if (player == NULL)
	{
		return;
	}

	const EnemyType& type = EnemyTypeOf(m_Data.kind);
	const bool warden = IsWarden();

	m_Data.hitFlash = std::max(0.0f, m_Data.hitFlash - deltaSeconds);
	m_Data.touchTimer -= deltaSeconds;
	m_Facing = (player->WorldX() >= WorldX()) ? 1.0f : -1.0f;

	const float playerX = player->WorldX();
	const float playerY = player->WorldY();
	const float toPlayer = Distance(WorldX(), WorldY(), playerX, playerY);

	// 너무 멀어진 적은 조용히 사라진다. 종탑지기도 다시 다가오면 온전한 몸으로 선다.
	if (toPlayer > kDespawnDistance)
	{
		Destroy();
		return;
	}

	if (warden)
	{
		const float playerToHome = Distance(playerX, playerY, m_Data.homeX, m_Data.homeY);
		if (!m_Data.engaged && playerToHome < kWardenAggro)
		{
			m_Data.engaged = true;
			m_Data.burstTimer = 1.6f;
		}
		else if (m_Data.engaged && playerToHome > kWardenLeash)
		{
			m_Data.engaged = false;
		}

		if (!m_Data.engaged)
		{
			m_Data.health = std::min(m_Data.maxHealth,
			                         m_Data.health + m_Data.maxHealth * 0.25f * deltaSeconds);
		}
	}
	else if (context.game->BeaconBrightness(WorldX(), WorldY()) > 0.0f)
	{
		// 방금 종이 울려 빛이 퍼졌다면, 그 안에 있던 어둠은 흩어진다.
		m_Data.dying = 0.001f;
		return;
	}

	// ── 어디로 갈 것인가 ──
	float targetX = playerX;
	float targetY = playerY;
	if (warden && !m_Data.engaged)
	{
		// 종탑 앞을 천천히 서성인다
		targetX = m_Data.homeX + std::sin(context.time * 0.4f + m_Data.phase) * 48.0f;
		targetY = m_Data.homeY + std::cos(context.time * 0.3f + m_Data.phase) * 16.0f;
	}

	const float dx = targetX - WorldX();
	const float dy = targetY - WorldY();
	const float length = std::sqrt(dx * dx + dy * dy);

	float speed = type.speed;
	if (warden && !m_Data.engaged)
	{
		speed *= 0.45f;
	}
	if (!warden && context.game->TotalBrightness(WorldX(), WorldY()) >= kRevealAt)
	{
		speed *= kLightSlow;
	}

	float moveX = 0.0f;
	float moveY = 0.0f;
	const float stopAt = (warden && !m_Data.engaged) ? 2.0f : type.radius + kPlayerRadius * 0.6f;
	if (length > stopAt)
	{
		moveX = dx / length * speed * deltaSeconds;
		moveY = dy / length * speed * deltaSeconds;
	}

	// 서로 겹치지 않게 밀어낸다. 무리가 한 점으로 뭉치면 한 마리처럼 보인다.
	const std::vector<EnemyActor*>& others = context.game->Enemies();
	for (size_t i = 0; i < others.size(); ++i)
	{
		if (others[i] == this || others[i]->IsDying())
		{
			continue;
		}
		const float ox = WorldX() - others[i]->WorldX();
		const float oy = WorldY() - others[i]->WorldY();
		const float gap = std::sqrt(ox * ox + oy * oy);
		const float minimum = type.radius + others[i]->Radius();
		if (gap > 0.01f && gap < minimum)
		{
			const float push = (minimum - gap) * 4.0f * deltaSeconds;
			moveX += ox / gap * push;
			moveY += oy / gap * push;
		}
	}

	// 불씨에 맞아 밀려난 만큼
	moveX += m_Data.knockX * deltaSeconds;
	moveY += m_Data.knockY * deltaSeconds;
	const float damping = std::exp(-8.0f * deltaSeconds);
	m_Data.knockX *= damping;
	m_Data.knockY *= damping;

	float x = WorldX();
	float y = WorldY();
	context.game->MoveBody(x, y, moveX, moveY, type.radius, !warden);
	SetPosition(x, y);

	// ── 붙으면 할퀸다 ──
	if ((!warden || m_Data.engaged) && m_Data.touchTimer <= 0.0f
	 && Distance(WorldX(), WorldY(), playerX, playerY) < type.radius + kPlayerRadius)
	{
		player->TakeDamage(context, type.touchDamage * (1.0f + 0.5f * (m_Data.power - 1.0f)),
		                   WorldX(), WorldY());
		m_Data.touchTimer = type.touchCooldown;
	}

	// ── 종탑지기: 어둠 구슬을 사방으로 흩뿌린다 ──
	// 집 벽에 막히므로, 집 뒤로 숨는 것이 피하는 방법이 된다.
	if (warden && m_Data.engaged)
	{
		m_Data.burstTimer -= deltaSeconds;
		if (m_Data.burstTimer <= 0.0f)
		{
			const float offset = context.time * 0.7f;
			const float damage = 12.0f * (1.0f + 0.5f * (m_Data.power - 1.0f));
			for (int k = 0; k < kWardenOrbs; ++k)
			{
				const float angle = offset + k * kTwoPi / kWardenOrbs;
				context.scene->Spawn(std::unique_ptr<Actor>(new OrbActor(
					WorldX(), WorldY() - 34.0f,
					std::cos(angle) * kOrbSpeed, std::sin(angle) * kOrbSpeed, damage, kOrbLife)));
			}
			m_Data.burstTimer = kWardenBurstEvery;
		}
	}
}

void EnemyActor::OnDraw(const DrawContext& context) const
{
	DrawEnemy(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	          m_Data, m_Facing, context.time);
}

void EnemyActor::TakeDamage(const ActorContext& context, float damage, float fromX, float fromY)
{
	if (m_Data.dying > 0.0f)
	{
		return;
	}

	m_Data.health -= damage;
	m_Data.hitFlash = 0.12f;

	// 맞은 반대쪽으로 밀려난다. 종탑지기는 거의 밀리지 않는다.
	float awayX = WorldX() - fromX;
	float awayY = BodyCenterY() - fromY;
	const float length = std::sqrt(awayX * awayX + awayY * awayY);
	if (length > 0.001f)
	{
		const float strength = IsWarden() ? 40.0f : 260.0f;
		m_Data.knockX += awayX / length * strength;
		m_Data.knockY += awayY / length * strength;
	}

	context.game->SpawnFloatingText(WorldX(), WorldY() - Radius() * 2.0f - 8.0f, std::string(),
	                                static_cast<int>(damage + 0.5f), kColorDamage,
	                                IsWarden() ? 17.0f : 15.0f);

	// 멀리서 쏴도 종탑지기는 깨어난다
	if (IsWarden() && !m_Data.engaged)
	{
		m_Data.engaged = true;
		m_Data.burstTimer = 1.2f;
	}

	if (m_Data.health <= 0.0f)
	{
		Die(context);
	}
}

void EnemyActor::Die(const ActorContext& context)
{
	m_Data.health = 0.0f;
	m_Data.dying = 0.001f;
	DropLoot(context);

	if (IsWarden())
	{
		context.game->NotifyWardenDefeated(m_Data.regionX, m_Data.regionY);
		context.game->SpawnFloatingText(WorldX(), WorldY() - 90.0f, "종탑지기가 쓰러졌다",
		                                -1, kColorGold, 20.0f);
	}
}

void EnemyActor::DropLoot(const ActorContext& context)
{
	const EnemyType& type = EnemyTypeOf(m_Data.kind);
	const bool warden = IsWarden();
	Game* game = context.game;

	auto drop = [&](PickupKind kind, int value)
	{
		const float angle = game->RandomRange(0.0f, kTwoPi);
		const float speed = game->RandomRange(60.0f, warden ? 260.0f : 150.0f);
		context.scene->Spawn(std::unique_ptr<Actor>(new PickupActor(
			kind, WorldX(), WorldY() - 4.0f,
			std::cos(angle) * speed, std::sin(angle) * speed, value)));
	};

	// 섬 안쪽의 적일수록 영혼이 짙다
	const int soulValue = std::max(1, static_cast<int>(type.soulValue * (0.5f + 0.5f * m_Data.power) + 0.5f));
	for (int i = 0; i < type.souls; ++i)
	{
		drop(PickupKind::Soul, soulValue);
	}

	if (game->Roll(type.healChance))
	{
		drop(PickupKind::Heal, 1);
	}

	if (game->Roll(type.weaponChance))
	{
		// 무기가 다 강해졌으면 강화석 대신 짙은 영혼석을 준다
		if (context.player != NULL && context.player->Combat().weaponTier < Progression::kMaxWeaponTier)
		{
			drop(PickupKind::Weapon, 1);
		}
		else
		{
			drop(PickupKind::Soul, soulValue * 3);
		}
	}

	if (game->Roll(type.magnetChance))
	{
		drop(PickupKind::Magnet, 1);
	}
}

// ═══ 불씨 ═════════════════════════════════════════════════════════
ProjectileActor::ProjectileActor(float worldX, float worldY, float velocityX, float velocityY,
                                 float damage, int pierce, float travel, int targetId)
	: Actor("Ember")
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Effect);
	m_Data.vx = velocityX;
	m_Data.vy = velocityY;
	m_Data.damage = damage;
	m_Data.pierce = pierce;
	m_Data.travel = travel;
	m_Data.targetId = targetId;
}

void ProjectileActor::OnUpdate(const ActorContext& context)
{
	const float speed = std::sqrt(m_Data.vx * m_Data.vx + m_Data.vy * m_Data.vy);

	// 쫓던 적이 살아 있으면 조금씩 방향을 틀어 따라간다 — 자동 조준의 뒷심이다.
	if (m_Data.targetId >= 0)
	{
		EnemyActor* target = context.game->FindEnemy(m_Data.targetId);
		if (target != NULL && !target->IsDying() && speed > 0.0f)
		{
			float wantX = target->WorldX() - WorldX();
			float wantY = target->BodyCenterY() - WorldY();
			const float wantLength = std::sqrt(wantX * wantX + wantY * wantY);
			if (wantLength > 0.001f)
			{
				const float turn = std::min(1.0f, 9.0f * context.deltaSeconds);
				float newX = m_Data.vx / speed + (wantX / wantLength - m_Data.vx / speed) * turn;
				float newY = m_Data.vy / speed + (wantY / wantLength - m_Data.vy / speed) * turn;
				const float newLength = std::sqrt(newX * newX + newY * newY);
				if (newLength > 0.001f)
				{
					m_Data.vx = newX / newLength * speed;
					m_Data.vy = newY / newLength * speed;
				}
			}
		}
		else
		{
			m_Data.targetId = -1;
		}
	}

	SetPosition(WorldX() + m_Data.vx * context.deltaSeconds,
	            WorldY() + m_Data.vy * context.deltaSeconds);
	m_Data.travel -= speed * context.deltaSeconds;

	const std::vector<EnemyActor*>& enemies = context.game->Enemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor* enemy = enemies[i];
		if (enemy->IsDying() || enemy->Id() == m_Data.lastHitId)
		{
			continue;
		}

		if (Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->BodyCenterY()) < enemy->Radius() + 4.0f)
		{
			enemy->TakeDamage(context, m_Data.damage, WorldX(), WorldY());
			m_Data.lastHitId = enemy->Id();
			m_Data.targetId = -1;
			if (--m_Data.pierce < 0)
			{
				m_Data.travel = 0.0f;
			}
			break;
		}
	}

	if (m_Data.travel <= 0.0f)
	{
		Destroy();
	}
}

void ProjectileActor::OnDraw(const DrawContext& context) const
{
	DrawEmber(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	          m_Data.vx, m_Data.vy);
}

// ═══ 어둠 구슬 ════════════════════════════════════════════════════
OrbActor::OrbActor(float worldX, float worldY, float velocityX, float velocityY, float damage, float life)
	: Actor("Orb")
	, m_Seed(worldX)
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Effect);
	m_Data.vx = velocityX;
	m_Data.vy = velocityY;
	m_Data.damage = damage;
	m_Data.life = life;
}

void OrbActor::OnUpdate(const ActorContext& context)
{
	m_Data.life -= context.deltaSeconds;
	SetPosition(WorldX() + m_Data.vx * context.deltaSeconds,
	            WorldY() + m_Data.vy * context.deltaSeconds);

	// 집 벽과 나무에 막힌다. 그래서 지형이 엄폐물이 된다.
	if (context.game->IsBlockedAtWorld(WorldX(), WorldY()))
	{
		Destroy();
		return;
	}

	PlayerActor* player = context.player;
	if (player != NULL
	 && Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY() - 14.0f) < 6.0f + kPlayerRadius)
	{
		player->TakeDamage(context, m_Data.damage, WorldX(), WorldY());
		Destroy();
		return;
	}

	if (m_Data.life <= 0.0f)
	{
		Destroy();
	}
}

void OrbActor::OnDraw(const DrawContext& context) const
{
	DrawOrb(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	        m_Seed, context.time);
}

// ═══ 떨어진 물건 ══════════════════════════════════════════════════
PickupActor::PickupActor(PickupKind kind, float worldX, float worldY,
                         float velocityX, float velocityY, int value)
	: Actor("Pickup")
	, m_Seed(worldX)
{
	SetPosition(worldX, worldY);
	m_Data.kind = kind;
	m_Data.vx = velocityX;
	m_Data.vy = velocityY;
	m_Data.value = value;
}

void PickupActor::OnUpdate(const ActorContext& context)
{
	PlayerActor* player = context.player;
	if (player == NULL)
	{
		return;
	}

	m_Data.age += context.deltaSeconds;

	// 흩어지며 떨어졌다가 멈춘다
	const float damping = std::exp(-5.0f * context.deltaSeconds);
	m_Data.vx *= damping;
	m_Data.vy *= damping;
	SetPosition(WorldX() + m_Data.vx * context.deltaSeconds,
	            WorldY() + m_Data.vy * context.deltaSeconds);

	const bool magnet = player->Combat().magnetTime > 0.0f;
	const float attract = magnet ? kMagnetRadius : Progression::PickupRadius(player->Combat().level);
	const float distance = Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY());

	// 가까우면 저절로 끌려온다. 자석이 켜져 있으면 멀리 있는 것까지 전부.
	if (!context.game->IsRespawning() && m_Data.age > kPickupSettle
	 && distance < attract && distance > 0.001f)
	{
		const float pull = magnet ? 560.0f : 260.0f + (attract - distance) * 2.0f;
		const float step = std::min(distance, pull * context.deltaSeconds);
		SetPosition(WorldX() + (player->WorldX() - WorldX()) / distance * step,
		            WorldY() + (player->WorldY() - WorldY()) / distance * step);
	}

	if (!context.game->IsRespawning()
	 && Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY()) < kPickupReach)
	{
		Collect(context);
		Destroy();
		return;
	}

	if (m_Data.age > kPickupLifetime)
	{
		Destroy();
	}
}

void PickupActor::Collect(const ActorContext& context)
{
	PlayerActor* player = context.player;
	const float headY = player->WorldY() - 44.0f;

	switch (m_Data.kind)
	{
	case PickupKind::Soul:
		context.game->SpawnFloatingText(player->WorldX(), headY, std::string(), m_Data.value, kColorSoul, 13.0f);
		player->GainSouls(context, m_Data.value);
		break;

	case PickupKind::Heal:
		player->Heal(player->MaxHealth() * 0.3f);
		context.game->SpawnFloatingText(player->WorldX(), headY, "체력 회복", -1, kColorHeal, 15.0f);
		break;

	case PickupKind::Weapon:
		if (player->Combat().weaponTier < Progression::kMaxWeaponTier)
		{
			player->AddWeaponTier();
			context.game->SpawnFloatingText(player->WorldX(), headY, "무기 강화", -1, kColorGold, 16.0f);
		}
		else
		{
			player->GainSouls(context, 5);
		}
		break;

	case PickupKind::Magnet:
		player->StartMagnet(kMagnetSeconds);
		context.game->SpawnFloatingText(player->WorldX(), headY, "자석", -1, kColorSoul, 15.0f);
		break;
	}
}

void PickupActor::OnDraw(const DrawContext& context) const
{
	DrawPickup(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	           m_Data, m_Seed, context.time);
}

// ═══ 떠오르는 글자 ════════════════════════════════════════════════
FloatingTextActor::FloatingTextActor(float worldX, float worldY, const std::string& label,
                                     int number, const float color[3], float size)
	: Actor("FloatingText")
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Overlay);
	m_Data.label = label;
	m_Data.number = number;
	m_Data.r = color[0];
	m_Data.g = color[1];
	m_Data.b = color[2];
	m_Data.size = size;
	m_Data.life = label.empty() ? 0.8f : 1.6f;
}

void FloatingTextActor::OnUpdate(const ActorContext& context)
{
	m_Data.age += context.deltaSeconds;
	Move(0.0f, -30.0f * context.deltaSeconds);   // 월드는 y가 아래로 커진다. 위로 떠오른다

	if (m_Data.age >= m_Data.life)
	{
		Destroy();
	}
}

void FloatingTextActor::OnDraw(const DrawContext& context) const
{
	// 글자는 후처리가 끝난 화면 위에 그려지므로 Game이 대신 그려 준다.
	context.game->DrawFloatingText(context, m_Data, WorldX(), WorldY());
}
