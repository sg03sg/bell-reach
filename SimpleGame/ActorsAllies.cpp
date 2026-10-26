#include "stdafx.h"
#include "Actors.h"

#include "Art.h"
#include "Game.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Skills.h"

#include <algorithm>
#include <cmath>

using namespace Art;

//
// 함께 싸우는 쪽.
//
// 동행자의 스킬, 마을 사람이 던지는 돌, 그리고 그것들이 남기는 효과와 장판.
// 적과 플레이어의 싸움(ActorsCombat.cpp)과 나눠 둔 것은, 이쪽이 전부
// "누구 편인가"를 따지는 코드이기 때문이다.
//

namespace
{
	const float kPi = 3.14159265f;
	const float kTwoPi = 6.2831853f;

	Color WithAlpha(const float c[3], float alpha)
	{
		const Color color = { c[0], c[1], c[2], alpha };
		return color;
	}

	// 땅에 그리는 고리. 둘레를 짧은 막대로 이어 그린다.
	// 화면이 위에서 내려다보는 시점이라 원은 원으로 그려야 맞는 범위가 보인다.
	void DrawGroundRing(Renderer* r, float cx, float cy, float radius, float thickness,
	                    const Color& color, float emissive, int segments = 32)
	{
		float prevX = cx + radius;
		float prevY = cy;
		for (int i = 1; i <= segments; ++i)
		{
			const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
			const float x = cx + std::cos(angle) * radius;
			const float y = cy + std::sin(angle) * radius;
			r->DrawSegment(prevX, prevY, x, y, thickness, color, emissive);
			prevX = x;
			prevY = y;
		}
	}

	// 둘레의 적을 한꺼번에 친다. push가 있으면 바깥으로 더 세게 민다.
	// 목록을 베껴 두고 도는 것은, 맞아 죽은 적이 전리품을 떨구며
	// 장면에 무언가를 놓아도 순회가 흔들리지 않게 하려는 것이다.
	int HitAround(const ActorContext& context, float x, float y, float radius,
	              float damage, float push)
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

	// 기준점에서 가장 가까운 적. 거리 안에 없으면 NULL.
	EnemyActor* NearestEnemy(const ActorContext& context, float x, float y, float within)
	{
		EnemyActor* best = NULL;
		float bestDistance = within;
		const std::vector<EnemyActor*>& enemies = context.game->Enemies();
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			if (enemies[i]->IsDying())
			{
				continue;
			}
			const float distance = Distance(x, y, enemies[i]->WorldX(), enemies[i]->BodyCenterY());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = enemies[i];
			}
		}
		return best;
	}
}

// ═══ 동행자의 스킬 ════════════════════════════════════════════════
//
// 쿨타임이 찼을 때 불린다. 조건이 맞으면 쓰고 true, 아니면 아무것도
// 안 하고 false — 그러면 쿨타임이 찬 채로 기다렸다가 적이 오면 바로 쓴다.
//
// 피해는 플레이어 레벨에 따라 커진다. 섬 안쪽의 적이 강해지는 만큼
// 동행자도 따라 강해져야, 깊이 들어가서도 데려간 보람이 있다.
//
bool CompanionActor::UseSkill(const ActorContext& context)
{
	Game* game = context.game;
	PlayerActor* player = context.player;
	const Skills::Info& skill = Skills::At(m_Profession);

	const float scale = 1.0f + 0.12f * static_cast<float>(player->Combat().level - 1);

	// 동행자의 몸 가운데. 여기서 스킬이 나간다.
	const float bodyX = WorldX();
	const float bodyY = WorldY() - 14.0f;

	EnemyActor* target = NearestEnemy(context, bodyX, bodyY, skill.reach);

	switch (Perks::Wrap(m_Profession))
	{
	case 0:   // 대장장이 — 망치 내려치기. 둘레를 한꺼번에 치고 밀어낸다
	{
		if (target == NULL)
		{
			return false;
		}
		HitAround(context, WorldX(), WorldY(), 85.0f, 20.0f * scale, 0.0f);
		context.scene->Spawn(std::unique_ptr<Actor>(new EffectActor(
			EffectActor::Kind::Shockwave, WorldX(), WorldY(), 85.0f, 0.38f, skill.tint)));
		break;
	}

	case 1:   // 광부 — 폭약. 적의 발밑에 던져 둘레를 날린다
	{
		if (target == NULL)
		{
			return false;
		}
		std::unique_ptr<ThrownActor> bomb(new ThrownActor(
			ThrownActor::Kind::Bomb, ThrownActor::Team::HitsEnemies,
			bodyX, bodyY, target->WorldX(), target->WorldY(), 330.0f, 32.0f * scale, 0.0f));
		bomb->SetBlast(80.0f);
		context.scene->Spawn(std::unique_ptr<Actor>(bomb.release()));
		break;
	}

	case 2:   // 수도승 — 기도. 싸우지 않고 플레이어를 살린다
	{
		// 다쳤을 때만 쓴다. 멀쩡할 때 쓰면 정작 필요할 때 쿨타임이다.
		if (player->Combat().health >= player->MaxHealth() * 0.8f || game->IsRespawning())
		{
			return false;
		}
		player->Heal(player->MaxHealth() * 0.18f);
		player->Shield(1.5f);

		std::unique_ptr<EffectActor> light(new EffectActor(
			EffectActor::Kind::Blessing, player->WorldX(), player->WorldY(), 40.0f, 0.9f, skill.tint));
		light->Follow(player);
		context.scene->Spawn(std::unique_ptr<Actor>(light.release()));
		break;
	}

	case 3:   // 약초꾼 — 독초 연기. 적 무리 위에 깔아 둔다
	{
		if (target == NULL)
		{
			return false;
		}
		context.scene->Spawn(std::unique_ptr<Actor>(new ZoneActor(
			ZoneActor::Kind::Poison, target->WorldX(), target->WorldY(),
			75.0f, 5.0f, 9.0f * scale, 0.7f)));
		break;
	}

	case 4:   // 사냥꾼 — 화살. 멀리 있는 적을 하나씩
	{
		if (target == NULL)
		{
			return false;
		}
		context.scene->Spawn(std::unique_ptr<Actor>(new ThrownActor(
			ThrownActor::Kind::Arrow, ThrownActor::Team::HitsEnemies,
			bodyX, bodyY, target->WorldX(), target->BodyCenterY(),
			720.0f, 24.0f * scale, skill.reach * 1.2f)));
		break;
	}

	case 5:   // 필경사 — 봉인의 글. 플레이어 둘레에 깔아 다가오는 적을 묶는다
	{
		EnemyActor* nearPlayer = NearestEnemy(context, player->WorldX(), player->WorldY() - 14.0f,
		                                      skill.reach);
		if (target == NULL && nearPlayer == NULL)
		{
			return false;
		}
		context.scene->Spawn(std::unique_ptr<Actor>(new ZoneActor(
			ZoneActor::Kind::Seal, player->WorldX(), player->WorldY(), 120.0f, 4.5f, 0.0f, 0.35f)));
		if (target == NULL)
		{
			target = nearPlayer;
		}
		break;
	}

	case 6:   // 종지기 노인 — 종소리. 둘레를 멀리 밀어낸다
	{
		if (target == NULL)
		{
			return false;
		}
		HitAround(context, WorldX(), WorldY(), 140.0f, 12.0f * scale, 480.0f);
		context.scene->Spawn(std::unique_ptr<Actor>(new EffectActor(
			EffectActor::Kind::BellRing, WorldX(), WorldY(), 140.0f, 0.65f, skill.tint)));
		break;
	}

	case 7:   // 떠돌이 병사 — 베기. 가까운 적을 빠르게
	{
		if (target == NULL)
		{
			return false;
		}
		target->TakeDamage(context, 16.0f * scale, bodyX, bodyY);

		std::unique_ptr<EffectActor> slash(new EffectActor(
			EffectActor::Kind::Slash, target->WorldX(), target->WorldY(), 26.0f, 0.2f, skill.tint));
		// 화면 좌표의 방향(y가 뒤집혀 있다)
		slash->SetAngle(std::atan2(-(target->WorldY() - WorldY()), target->WorldX() - WorldX()));
		context.scene->Spawn(std::unique_ptr<Actor>(slash.release()));
		break;
	}

	default:
		return false;
	}

	// 스킬을 쓴 쪽을 잠깐 바라본다
	if (target != NULL)
	{
		m_Facing = (target->WorldX() >= WorldX()) ? 1.0f : -1.0f;
		m_FacingHold = 0.6f;
	}

	if (skill.announce)
	{
		game->SpawnFloatingText(WorldX(), WorldY() - 58.0f, skill.name, -1, skill.tint, 14.0f);
	}
	return true;
}

// ═══ 던지는 것 ════════════════════════════════════════════════════
ThrownActor::ThrownActor(Kind kind, Team team, float worldX, float worldY,
                         float targetX, float targetY, float speed, float damage, float range)
	: Actor((kind == Kind::Arrow || kind == Kind::Light) ? "Arrow"
	        : ((kind == Kind::Bomb || kind == Kind::Fireball) ? "Bomb" : "Stone"))
	, m_Kind(kind)
	, m_Team(team)
	, m_Damage(damage)
	, m_Range(range)
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Effect);

	float dx = targetX - worldX;
	float dy = targetY - worldY;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length > 0.001f)
	{
		dx /= length;
		dy /= length;
	}
	else
	{
		dx = 1.0f;
		dy = 0.0f;
	}
	m_VX = dx * speed;
	m_VY = dy * speed;

	// 폭약과 불덩이는 과녁 자리까지만 날아가 떨어진다
	if (kind == Kind::Bomb || kind == Kind::Fireball)
	{
		m_Range = std::max(24.0f, length);
	}
}

void ThrownActor::OnUpdate(const ActorContext& context)
{
	const float deltaSeconds = context.deltaSeconds;
	const float speed = std::sqrt(m_VX * m_VX + m_VY * m_VY);

	SetPosition(WorldX() + m_VX * deltaSeconds, WorldY() + m_VY * deltaSeconds);
	m_Flown += speed * deltaSeconds;
	m_Spin += deltaSeconds * 13.0f;

	if (m_Kind == Kind::Bomb || m_Kind == Kind::Fireball)
	{
		if (m_Flown >= m_Range)
		{
			Explode(context);
			Destroy();
		}
		return;
	}

	// 맞으면 날아온 쪽에서 밀려나도록, 조금 뒤의 자리를 "맞은 곳"으로 넘긴다.
	const float fromX = WorldX() - m_VX * 0.02f;
	const float fromY = WorldY() - m_VY * 0.02f;

	if (m_Team == Team::HitsEnemies)
	{
		const std::vector<EnemyActor*>& enemies = context.game->Enemies();
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			EnemyActor* enemy = enemies[i];
			if (enemy->IsDying())
			{
				continue;
			}
			if (Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->BodyCenterY())
			    < enemy->Radius() + 4.0f)
			{
				// 마을 사람이 던졌으면 누가 맞혔는지 적어 둔다. 쓰러지면 현상금을 받는다.
				enemy->MarkHitBy(m_Owner);
				enemy->TakeDamage(context, m_Damage, fromX, fromY);
				Destroy();
				return;
			}
		}
	}
	else if (m_Team == Team::HitsVillager)
	{
		// 마을 사람끼리의 다툼. 노린 사람만 맞고, 다른 사람은 그냥 지나친다.
		VillagerActor* target = context.game->FindVillager(m_Target);
		if (target != NULL && !target->IsDead()
		    && Distance(WorldX(), WorldY(), target->WorldX(), target->BodyCenterY()) < target->Radius() + 4.0f)
		{
			target->TakeHitFrom(context, m_Damage, m_Owner);
			Destroy();
			return;
		}
	}
	else
	{
		PlayerActor* player = context.player;
		if (player != NULL
		    && Distance(WorldX(), WorldY(), player->WorldX(), player->WorldY() - 12.0f) < 14.0f)
		{
			player->TakeDamage(context, m_Damage, fromX, fromY);
			Destroy();
			return;
		}
	}

	if (m_Flown >= m_Range)
	{
		Destroy();
	}
}

void ThrownActor::Explode(const ActorContext& context)
{
	// 한가운데일수록 세게. 가장자리는 60%까지 줄어든다.
	const std::vector<EnemyActor*> enemies = context.game->Enemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor* enemy = enemies[i];
		if (enemy->IsDying())
		{
			continue;
		}
		const float distance = Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->WorldY());
		if (distance > m_Blast + enemy->Radius())
		{
			continue;
		}
		const float falloff = 1.0f - 0.4f * std::min(1.0f, distance / m_Blast);
		enemy->TakeDamage(context, m_Damage * falloff, WorldX(), WorldY());
	}

	// 마법사의 불덩이는 보랏빛이 도는 불로 터진다
	const float powder[3] = { 1.0f, 0.62f, 0.25f };
	const float arcane[3] = { 0.78f, 0.52f, 1.0f };
	context.scene->Spawn(std::unique_ptr<Actor>(new EffectActor(
		EffectActor::Kind::Blast, WorldX(), WorldY(), m_Blast, 0.55f,
		(m_Kind == Kind::Fireball) ? arcane : powder)));
}

void ThrownActor::OnDraw(const DrawContext& context) const
{
	Renderer* r = context.renderer;
	const float x = context.ToRenderX(WorldX());
	const float y = context.ToRenderY(WorldY());

	switch (m_Kind)
	{
	case Kind::Stone:
	{
		// 마을 사람의 돌. 원한을 품고 던진 돌은 붉은 기가 돈다.
		const bool angry = (m_Team == Team::HitsPlayer);
		const Color stone = angry ? Color{ 0.62f, 0.38f, 0.34f, 1.0f } : Tint(kStone, 0.95f);
		r->DrawEllipse(x, y, 3.6f, 3.0f, stone, m_Spin);
		r->DrawCircle(x - 0.9f, y + 0.9f, 1.1f, Tint(kStone, 1.25f, 0.8f));
		break;
	}

	case Kind::Arrow:
	{
		// 화면 좌표의 날아가는 방향(y가 뒤집혀 있다)
		const float angle = std::atan2(-m_VY, m_VX);
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		r->DrawSegment(x - c * 14.0f, y - s * 14.0f, x, y, 1.8f, Tint(kWood, 1.1f));
		r->DrawTriangle(x + c * 2.0f, y + s * 2.0f, 5.0f, 6.0f, Tint(kSteel, 1.1f), angle - kPi * 0.5f);
		r->DrawSegment(x - c * 14.0f, y - s * 14.0f, x - c * 10.0f + s * 2.5f, y - s * 10.0f - c * 2.5f,
		               1.2f, Tint(kHair, 1.3f));
		break;
	}

	case Kind::Bomb:
	{
		// 포물선. 땅에는 그림자를, 그 위 높이에 폭약을 그린다.
		const float t = std::min(1.0f, m_Flown / std::max(1.0f, m_Range));
		const float height = std::sin(kPi * t) * std::min(64.0f, m_Range * 0.3f);
		DrawShadow(r, x, y, 6.0f, 0.3f);
		r->DrawCircle(x, y + height, 5.2f, Color{ 0.16f, 0.13f, 0.12f, 1.0f });
		r->DrawRoundRect(x + 2.5f, y + height + 5.0f, 2.0f, 4.0f, 0.8f, Tint(kRope, 1.0f), 0.4f);

		// 타들어가는 심지
		const float flicker = 0.7f + 0.3f * std::sin(context.time * 40.0f);
		const float spark[3] = { 1.0f, 0.75f, 0.3f };
		r->DrawCircle(x + 3.4f, y + height + 7.8f, 2.0f, WithAlpha(spark, 1.0f), 2.2f * flicker);
		break;
	}

	case Kind::Light:
	{
		// 힐러의 빛 조각 — 짧은 금빛 꼬리를 끄는 낱알
		const float angle = std::atan2(-m_VY, m_VX);
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		const float gold[3] = { 1.0f, 0.92f, 0.62f };
		r->DrawSegment(x - c * 12.0f, y - s * 12.0f, x, y, 2.4f, WithAlpha(gold, 0.45f), 0.8f);
		r->DrawCircle(x, y, 3.2f, WithAlpha(gold, 1.0f), 2.2f);
		break;
	}

	case Kind::Fireball:
	{
		// 마법사의 불덩이 — 곧게 날아가며 일렁인다
		const float angle = std::atan2(-m_VY, m_VX);
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		const float flicker = 0.8f + 0.2f * std::sin(context.time * 31.0f);
		const float outer[3] = { 0.62f, 0.36f, 1.0f };
		const float inner[3] = { 1.0f, 0.78f, 0.52f };
		r->DrawCircle(x - c * 9.0f, y - s * 9.0f, 4.0f, WithAlpha(outer, 0.35f), 0.8f, 3.0f);
		r->DrawCircle(x, y + 2.0f, 7.0f * flicker, WithAlpha(outer, 0.8f), 1.8f, 2.0f);
		r->DrawCircle(x, y + 2.0f, 3.4f, WithAlpha(inner, 1.0f), 2.6f);
		break;
	}
	}
}

// ═══ 보이기만 하는 효과 ═══════════════════════════════════════════
EffectActor::EffectActor(Kind kind, float worldX, float worldY, float radius, float life,
                         const float tint[3])
	: Actor("Effect")
	, m_Kind(kind)
	, m_Radius(radius)
	, m_Life(life)
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Effect);
	m_Tint[0] = tint[0];
	m_Tint[1] = tint[1];
	m_Tint[2] = tint[2];
}

void EffectActor::OnUpdate(const ActorContext& context)
{
	m_Age += context.deltaSeconds;
	if (m_Follow != NULL)
	{
		SetPosition(m_Follow->WorldX(), m_Follow->WorldY());
	}
	if (m_Age >= m_Life)
	{
		Destroy();
	}
}

void EffectActor::OnDraw(const DrawContext& context) const
{
	Renderer* r = context.renderer;
	const float x = context.ToRenderX(WorldX());
	const float y = context.ToRenderY(WorldY());
	const float t = std::min(1.0f, m_Age / m_Life);
	const float fade = 1.0f - t;
	const float easeOut = 1.0f - (1.0f - t) * (1.0f - t);

	switch (m_Kind)
	{
	case Kind::Shockwave:
	{
		// 망치가 땅을 친 자리에서 퍼지는 고리와 흙먼지
		const float radius = m_Radius * (0.25f + 0.75f * easeOut);
		r->DrawCircle(x, y, radius, WithAlpha(m_Tint, 0.14f * fade), 0.3f * fade, 8.0f);
		DrawGroundRing(r, x, y, radius, 1.0f + 4.0f * fade, WithAlpha(m_Tint, 0.85f * fade), 1.3f * fade);

		const float dust[3] = { 0.42f, 0.36f, 0.30f };
		for (int i = 0; i < 6; ++i)
		{
			const float angle = kTwoPi * static_cast<float>(i) / 6.0f + 0.4f;
			r->DrawCircle(x + std::cos(angle) * radius * 0.9f,
			              y + std::sin(angle) * radius * 0.9f + t * 10.0f,
			              4.0f + t * 5.0f, WithAlpha(dust, 0.4f * fade), 0.0f, 3.0f);
		}
		break;
	}

	case Kind::BellRing:
	{
		// 종소리가 겹겹이 퍼진다. 뒤의 고리는 조금 늦게 떠난다.
		for (int ring = 0; ring < 2; ++ring)
		{
			const float local = t - static_cast<float>(ring) * 0.25f;
			if (local <= 0.0f)
			{
				continue;
			}
			const float k = std::min(1.0f, local / 0.75f);
			const float radius = m_Radius * (0.2f + 0.8f * (1.0f - (1.0f - k) * (1.0f - k)));
			DrawGroundRing(r, x, y, radius, 1.5f + 3.0f * (1.0f - k),
			               WithAlpha(m_Tint, 0.9f * (1.0f - k)), 1.8f * (1.0f - k), 40);
		}
		if (t < 0.3f)
		{
			r->DrawCircle(x, y + 18.0f, 26.0f, WithAlpha(m_Tint, 0.35f * (1.0f - t / 0.3f)),
			              1.6f * (1.0f - t / 0.3f), 14.0f);
		}
		break;
	}

	case Kind::Slash:
	{
		// 적의 몸 가운데를 가로지르는 초승달
		const float cx = x;
		const float cy = y + 14.0f;
		const float span = 2.0f;
		const int segments = 8;
		float prevX = cx + std::cos(m_Angle - span * 0.5f) * m_Radius;
		float prevY = cy + std::sin(m_Angle - span * 0.5f) * m_Radius;
		for (int i = 1; i <= segments; ++i)
		{
			const float u = static_cast<float>(i) / static_cast<float>(segments);
			const float angle = m_Angle - span * 0.5f + span * u;
			const float px = cx + std::cos(angle) * m_Radius;
			const float py = cy + std::sin(angle) * m_Radius;
			const float thickness = 1.0f + 3.5f * std::sin(kPi * u);
			r->DrawSegment(prevX, prevY, px, py, thickness, WithAlpha(m_Tint, fade), 0.9f * fade);
			prevX = px;
			prevY = py;
		}
		break;
	}

	case Kind::Blast:
	{
		// 번쩍임, 퍼지는 고리, 피어오르는 연기
		r->DrawCircle(x, y + 8.0f, m_Radius * 0.6f * (1.0f - t * 0.4f),
		              WithAlpha(m_Tint, 0.9f * fade * fade), 2.2f * fade, 12.0f);
		DrawGroundRing(r, x, y, m_Radius * (0.3f + 0.7f * easeOut), 3.0f * fade + 1.0f,
		               WithAlpha(m_Tint, 0.8f * fade), 1.2f * fade);

		const float smoke[3] = { 0.18f, 0.16f, 0.15f };
		for (int i = 0; i < 5; ++i)
		{
			const float angle = kTwoPi * static_cast<float>(i) / 5.0f;
			r->DrawCircle(x + std::cos(angle) * m_Radius * 0.35f,
			              y + 10.0f + std::sin(angle) * 8.0f + t * 34.0f,
			              10.0f + t * 12.0f, WithAlpha(smoke, 0.4f * fade), 0.0f, 6.0f);
		}
		break;
	}

	case Kind::Blessing:
	{
		// 발밑에서 번지는 금빛, 위로 솟는 빛기둥과 낱알
		r->DrawEllipse(x, y, 18.0f + 22.0f * easeOut, 7.0f + 8.0f * easeOut,
		               WithAlpha(m_Tint, 0.55f * fade), 0.0f, 1.2f * fade, 4.0f);
		r->DrawRoundRect(x, y + 34.0f, 24.0f * (1.0f - t * 0.5f), 76.0f, 10.0f,
		                 WithAlpha(m_Tint, 0.22f * fade), 0.0f, 0.9f * fade);
		for (int i = 0; i < 6; ++i)
		{
			const float phase = static_cast<float>(i) * 1.7f;
			r->DrawCircle(x + std::sin(phase + t * 5.0f) * 14.0f,
			              y + 6.0f + t * 64.0f + static_cast<float>(i) * 7.0f,
			              1.8f, WithAlpha(m_Tint, fade), 2.0f * fade);
		}
		break;
	}
	}
}

// ═══ 땅에 깔리는 것 ═══════════════════════════════════════════════
ZoneActor::ZoneActor(Kind kind, float worldX, float worldY, float radius, float life,
                     float damagePerSecond, float slowFactor)
	: Actor("Zone")
	, m_Kind(kind)
	, m_Radius(radius)
	, m_Life(life)
	, m_DamagePerSecond(damagePerSecond)
	, m_SlowFactor(slowFactor)
{
	SetPosition(worldX, worldY);
	SetLayer(Layer::Object);   // SortDepth를 아주 작게 둬서 서 있는 것들 밑에 깔린다
}

void ZoneActor::OnUpdate(const ActorContext& context)
{
	m_Age += context.deltaSeconds;
	if (m_Age >= m_Life)
	{
		Destroy();
		return;
	}

	m_Tick -= context.deltaSeconds;
	const bool tick = (m_Tick <= 0.0f);
	if (tick)
	{
		m_Tick += 0.5f;
	}

	const std::vector<EnemyActor*> enemies = context.game->Enemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor* enemy = enemies[i];
		if (enemy->IsDying())
		{
			continue;
		}
		if (Distance(WorldX(), WorldY(), enemy->WorldX(), enemy->WorldY()) > m_Radius + enemy->Radius())
		{
			continue;
		}

		if (m_SlowFactor < 1.0f)
		{
			enemy->ApplySlow(m_SlowFactor, 0.4f);   // 나가면 금세 풀린다
		}

		// 독은 밀어내지 않는다. 맞은 곳을 적 자신의 자리로 넘기면 밀리지 않는다.
		if (tick && m_DamagePerSecond > 0.0f)
		{
			enemy->TakeDamage(context, m_DamagePerSecond * 0.5f, enemy->WorldX(), enemy->BodyCenterY());
		}
	}
}

void ZoneActor::OnDraw(const DrawContext& context) const
{
	Renderer* r = context.renderer;
	const float x = context.ToRenderX(WorldX());
	const float y = context.ToRenderY(WorldY());

	// 깔릴 때 피어나고, 사라질 때 잦아든다
	const float fadeIn = std::min(1.0f, m_Age / 0.3f);
	const float fadeOut = std::min(1.0f, (m_Life - m_Age) / 0.5f);
	const float fade = fadeIn * fadeOut;
	const float time = context.time;

	if (m_Kind == Kind::Poison)
	{
		const float base[3] = { 0.36f, 0.52f, 0.16f };
		const float puff[3] = { 0.56f, 0.76f, 0.26f };
		r->DrawCircle(x, y, m_Radius, WithAlpha(base, 0.16f * fade), 0.0f, 10.0f);
		for (int i = 0; i < 7; ++i)
		{
			const float fi = static_cast<float>(i);
			const float px = x + std::cos(fi * 2.4f + time * 0.35f) * m_Radius * 0.55f;
			const float py = y + std::sin(fi * 1.7f + time * 0.45f) * m_Radius * 0.55f + 6.0f;
			r->DrawCircle(px, py, m_Radius * 0.34f, WithAlpha(puff, 0.22f * fade), 0.15f, 9.0f);
		}
		return;
	}

	// 봉인의 글 — 두 겹 고리 위로 글자가 돈다
	const float ink[3] = { 0.62f, 0.56f, 1.0f };
	r->DrawCircle(x, y, m_Radius, WithAlpha(ink, 0.08f * fade), 0.0f, 8.0f);
	DrawGroundRing(r, x, y, m_Radius, 2.4f, WithAlpha(ink, 0.7f * fade), 0.9f * fade, 40);
	DrawGroundRing(r, x, y, m_Radius * 0.72f, 1.2f, WithAlpha(ink, 0.45f * fade), 0.6f * fade, 32);
	for (int i = 0; i < 8; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / 8.0f + time * 0.6f;
		const float gx = x + std::cos(angle) * m_Radius * 0.86f;
		const float gy = y + std::sin(angle) * m_Radius * 0.86f;
		r->DrawRoundRect(gx, gy, 3.6f, 7.5f, 1.0f, WithAlpha(ink, 0.9f * fade), angle, 1.1f * fade);
	}
}
