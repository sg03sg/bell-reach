#include "stdafx.h"
#include "Game.h"

#include "Input.h"
#include "Progression.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

//
// 전투의 규칙. 그리기는 Game.cpp에 있다.
//
//   어둠에서 적이 나타난다 → F를 누르면 가까운 적에게 불씨가 날아간다(자동 조준)
//   → 쓰러진 자리에 영혼석과 물건이 떨어진다 → 주우면 레벨이 오르고 강해진다
//   → 강해지면 종탑을 지키는 종탑지기를 쓰러뜨리고 종을 울린다
//

namespace
{
	const int   kTileSize = World::kTilePixels;
	const float kTwoPi = 6.2831853f;

	const float kPlayerRadius = 10.0f;

	// ── 적이 나타나는 규칙 ──
	const float kSpawnInterval = 2.4f;     // 무리가 나타나는 평균 간격(초)
	const float kSpawnNear = 430.0f;       // 화면 밖, 등불이 닿지 않는 거리에서
	const float kSpawnFar = 640.0f;
	const float kSpawnDarkness = 0.2f;     // 이보다 밝은 곳에서는 나타나지 않는다
	const float kDespawnDistance = 1500.0f;
	const float kLightSlow = 0.6f;         // 불빛 안에서는 발이 무거워진다

	// ── 종탑지기 ──
	const float kWardenAppear = 900.0f;    // 이 거리 안에 들어오면 종탑 앞에 모습을 드러낸다
	const float kWardenAggro = 260.0f;     // 종탑에 이만큼 다가오면 덤벼든다
	const float kWardenLeash = 650.0f;     // 이만큼 멀어지면 제자리로 돌아가 상처를 회복한다
	const float kWardenBurstEvery = 3.0f;
	const int   kWardenOrbs = 12;
	const float kOrbSpeed = 175.0f;
	const float kOrbLife = 3.2f;

	// ── 플레이어 ──
	const float kInvulnerableTime = 0.55f; // 맞은 뒤 잠깐 무적
	const float kRespawnDelay = 1.4f;

	// ── 떨어진 것 ──
	const float kMagnetSeconds = 10.0f;
	const float kMagnetRadius = 900.0f;
	const float kPickupReach = 18.0f;
	const float kPickupLifetime = 90.0f;
	const float kPickupSettle = 0.35f;     // 흩어진 뒤 잠깐은 끌려오지 않는다 — 떨어지는 모습이 보이게

	// 글자 색
	const float kColorDamage[3] = { 1.00f, 0.86f, 0.62f };
	const float kColorHurt[3]   = { 1.00f, 0.36f, 0.30f };
	const float kColorSoul[3]   = { 0.62f, 0.82f, 1.00f };
	const float kColorHeal[3]   = { 0.50f, 0.95f, 0.58f };
	const float kColorGold[3]   = { 1.00f, 0.82f, 0.40f };

	float TileToWorld(int tile)
	{
		return tile * kTileSize + kTileSize * 0.5f;
	}

	float Distance(float ax, float ay, float bx, float by)
	{
		const float dx = ax - bx;
		const float dy = ay - by;
		return std::sqrt(dx * dx + dy * dy);
	}

	// 적의 발밑에서 몸 가운데까지. 월드는 y가 아래로 커지므로 위쪽은 빼기다.
	float BodyCenterY(const Enemy& enemy)
	{
		return enemy.y - EnemyTypeOf(enemy.kind).radius;
	}
}

float Game::RandomRange(float low, float high)
{
	std::uniform_real_distribution<float> distribution(low, high);
	return distribution(m_Random);
}

bool Game::Roll(int percent)
{
	return RandomRange(0.0f, 100.0f) < static_cast<float>(percent);
}

float Game::PowerAt(float worldX, float worldY) const
{
	// 처음 마을에서 멀어질수록 적이 강해진다. 마을 하나를 건널 때마다 18%.
	const int regionX = Regions::RegionOfTile(static_cast<int>(std::floor(worldX / kTileSize)));
	const int regionY = Regions::RegionOfTile(static_cast<int>(std::floor(worldY / kTileSize)));
	// 이름을 far로 하지 않는다. Windows 헤더에 같은 이름의 매크로가 있다.
	const int villagesAway = std::max(std::abs(regionX), std::abs(regionY));
	return 1.0f + 0.18f * static_cast<float>(villagesAway);
}

bool Game::TryMoveBody(float& x, float& y, float dx, float dy, float radius, bool stayOutOfBeacons) const
{
	auto blocked = [&](float px, float py)
	{
		const float half = radius * 0.7f;
		if (IsBlockedAtWorld(px - half, py - half) || IsBlockedAtWorld(px + half, py - half)
		 || IsBlockedAtWorld(px - half, py + half) || IsBlockedAtWorld(px + half, py + half))
		{
			return true;
		}
		// 종소리가 닿는 곳에는 어둠이 들어오지 못한다.
		return stayOutOfBeacons && BeaconBrightness(px, py) > 0.0f;
	};

	bool moved = false;
	if (dx != 0.0f && !blocked(x + dx, y)) { x += dx; moved = true; }
	if (dy != 0.0f && !blocked(x, y + dy)) { y += dy; moved = true; }
	return moved;
}

const Enemy* Game::FindEnemy(int id) const
{
	for (const Enemy& enemy : m_Enemies)
	{
		if (enemy.id == id)
		{
			return &enemy;
		}
	}
	return NULL;
}

void Game::AddFloatingText(float worldX, float worldY, const std::string& label, int number,
                           const float color[3], float size)
{
	FloatingText text;
	text.x = worldX + RandomRange(-6.0f, 6.0f);
	text.y = worldY;
	text.label = label;
	text.number = number;
	text.r = color[0];
	text.g = color[1];
	text.b = color[2];
	text.size = size;
	text.life = label.empty() ? 0.8f : 1.6f;
	m_FloatingTexts.push_back(text);
}

void Game::UpdateCombat(float deltaSeconds, const Input& input)
{
	m_Combat.invulnerable = std::max(0.0f, m_Combat.invulnerable - deltaSeconds);
	m_Combat.hurtFlash = std::max(0.0f, m_Combat.hurtFlash - deltaSeconds * 2.5f);
	m_Combat.magnetTime = std::max(0.0f, m_Combat.magnetTime - deltaSeconds);
	m_Combat.levelUpGlow = std::max(0.0f, m_Combat.levelUpGlow - deltaSeconds * 0.8f);
	m_Combat.attackCooldown = std::max(0.0f, m_Combat.attackCooldown - deltaSeconds);

	// 체력바 뒤로 따라오는 흰 막대. 한 번에 깎인 양이 눈에 보이게 천천히 줄어든다.
	if (m_Combat.healthShown > m_Combat.health)
	{
		m_Combat.healthShown = std::max(m_Combat.health, m_Combat.healthShown - deltaSeconds * 60.0f);
	}
	else
	{
		m_Combat.healthShown = m_Combat.health;
	}

	SpawnWardens();
	SpawnEnemies(deltaSeconds);
	UpdateEnemies(deltaSeconds);

	if (!m_PendingRespawn)
	{
		FireAtEnemies(input.Attack());
	}

	UpdateProjectiles(deltaSeconds);
	UpdateOrbs(deltaSeconds);
	UpdatePickups(deltaSeconds);

	for (FloatingText& text : m_FloatingTexts)
	{
		text.age += deltaSeconds;
		text.y -= 30.0f * deltaSeconds;
	}
	m_FloatingTexts.erase(std::remove_if(m_FloatingTexts.begin(), m_FloatingTexts.end(),
	                                     [](const FloatingText& t) { return t.age >= t.life; }),
	                      m_FloatingTexts.end());

	// 흩어지는 모습을 다 보인 적은 치운다.
	m_Enemies.erase(std::remove_if(m_Enemies.begin(), m_Enemies.end(),
	                               [](const Enemy& e) { return e.dying > 0.45f; }),
	                m_Enemies.end());

	// 쓰러졌으면 잠시 뒤 다시 일어선다. 목록을 도는 도중에 비우지 않도록 여기서 한다.
	if (m_PendingRespawn)
	{
		m_RespawnTimer -= deltaSeconds;
		if (m_RespawnTimer <= 0.0f)
		{
			RespawnPlayer();
		}
	}
}

void Game::SpawnWardens()
{
	for (const Regions::Region& region : m_NearbyRegions)
	{
		if (region.rung || region.wardenDefeated)
		{
			continue;
		}

		const float towerX = TileToWorld(region.towerTileX);
		const float towerY = TileToWorld(region.towerTileY);
		if (Distance(m_PlayerX, m_PlayerY, towerX, towerY) > kWardenAppear)
		{
			continue;
		}

		bool present = false;
		for (const Enemy& enemy : m_Enemies)
		{
			if (enemy.kind == EnemyKind::Warden && enemy.regionX == region.regionX
			 && enemy.regionY == region.regionY && enemy.dying == 0.0f)
			{
				present = true;
				break;
			}
		}
		if (present)
		{
			continue;
		}

		// 종탑 앞 광장에 선다. 멀리서도 보이도록 — 준비가 될 때까지 다가가지 말라는 경고다.
		const EnemyType& type = EnemyTypeOf(EnemyKind::Warden);
		Enemy warden;
		warden.id = m_NextEnemyId++;
		warden.kind = EnemyKind::Warden;
		warden.regionX = region.regionX;
		warden.regionY = region.regionY;
		warden.homeX = towerX;
		warden.homeY = towerY + 72.0f;
		warden.x = warden.homeX;
		warden.y = warden.homeY;
		warden.power = PowerAt(towerX, towerY);
		warden.maxHealth = type.health * warden.power;
		warden.health = warden.maxHealth;
		warden.phase = RandomRange(0.0f, kTwoPi);
		warden.burstTimer = kWardenBurstEvery;
		m_Enemies.push_back(warden);
	}
}

void Game::SpawnEnemies(float deltaSeconds)
{
	m_SpawnTimer -= deltaSeconds;
	if (m_SpawnTimer > 0.0f || m_PendingRespawn)
	{
		return;
	}
	m_SpawnTimer = kSpawnInterval * RandomRange(0.7f, 1.3f);

	// 되찾은 구역 안에 서 있으면 어둠이 다가오지 않는다.
	if (BeaconBrightness(m_PlayerX, m_PlayerY) > 0.0f)
	{
		return;
	}

	int alive = 0;
	for (const Enemy& enemy : m_Enemies)
	{
		if (enemy.kind != EnemyKind::Warden && enemy.dying == 0.0f)
		{
			++alive;
		}
	}

	const float power = PowerAt(m_PlayerX, m_PlayerY);
	const int villagesAway = static_cast<int>((power - 1.0f) / 0.18f + 0.5f);
	const int limit = std::min(16, 5 + m_Combat.level / 2 + villagesAway);
	if (alive >= limit)
	{
		return;
	}

	// 처음 마을에서 레벨이 낮을 때는 약한 그림자만 나온다. 파밍을 익히는 곳이다.
	const bool practice = (villagesAway == 0 && m_Combat.level < 3);
	const int huskPercent = practice ? 0 : std::min(60, 22 + villagesAway * 6 + m_Combat.level);

	const int group = std::min(limit - alive, 1 + static_cast<int>(RandomRange(0.0f, 2.99f)));
	for (int g = 0; g < group; ++g)
	{
		for (int attempt = 0; attempt < 10; ++attempt)
		{
			const float angle = RandomRange(0.0f, kTwoPi);
			const float distance = RandomRange(kSpawnNear, kSpawnFar);
			const float x = m_PlayerX + std::cos(angle) * distance;
			const float y = m_PlayerY + std::sin(angle) * distance;

			// 걸을 수 있는 칸에만 — 맵 생성이 모든 걸을 수 있는 칸을 길과 이어 두었으므로
			// 여기서 나타난 적은 반드시 플레이어에게 닿을 수 있다.
			if (IsBlockedAtWorld(x, y) || BeaconBrightness(x, y) > 0.0f
			 || TotalBrightness(x, y) >= kSpawnDarkness)
			{
				continue;
			}

			Enemy enemy;
			enemy.id = m_NextEnemyId++;
			enemy.kind = Roll(huskPercent) ? EnemyKind::Husk : EnemyKind::Wraith;
			enemy.x = x;
			enemy.y = y;
			enemy.power = PowerAt(x, y);
			enemy.maxHealth = EnemyTypeOf(enemy.kind).health * enemy.power;
			enemy.health = enemy.maxHealth;
			enemy.phase = RandomRange(0.0f, kTwoPi);
			enemy.touchTimer = 0.5f;
			m_Enemies.push_back(enemy);
			break;
		}
	}
}

void Game::UpdateEnemies(float deltaSeconds)
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		Enemy& enemy = m_Enemies[i];

		if (enemy.dying > 0.0f)
		{
			enemy.dying += deltaSeconds;
			continue;
		}

		const EnemyType& type = EnemyTypeOf(enemy.kind);
		const bool warden = (enemy.kind == EnemyKind::Warden);

		enemy.hitFlash = std::max(0.0f, enemy.hitFlash - deltaSeconds);
		enemy.touchTimer -= deltaSeconds;

		const float toPlayer = Distance(enemy.x, enemy.y, m_PlayerX, m_PlayerY);

		// 너무 멀어진 적은 조용히 사라진다. 종탑지기도 다시 다가오면 온전한 몸으로 선다.
		if (toPlayer > kDespawnDistance)
		{
			enemy.dying = 1.0f;
			continue;
		}

		if (warden)
		{
			const float playerToHome = Distance(m_PlayerX, m_PlayerY, enemy.homeX, enemy.homeY);
			if (!enemy.engaged && playerToHome < kWardenAggro)
			{
				enemy.engaged = true;
				enemy.burstTimer = 1.6f;
			}
			else if (enemy.engaged && playerToHome > kWardenLeash)
			{
				enemy.engaged = false;
			}

			if (!enemy.engaged)
			{
				enemy.health = std::min(enemy.maxHealth, enemy.health + enemy.maxHealth * 0.25f * deltaSeconds);
			}
		}
		else if (BeaconBrightness(enemy.x, enemy.y) > 0.0f)
		{
			// 방금 종이 울려 빛이 퍼졌다면, 그 안에 있던 어둠은 흩어진다.
			enemy.dying = 0.001f;
			continue;
		}

		// ── 어디로 갈 것인가 ──
		float targetX = m_PlayerX;
		float targetY = m_PlayerY;
		if (warden && !enemy.engaged)
		{
			// 종탑 앞을 천천히 서성인다
			targetX = enemy.homeX + std::sin(m_Time * 0.4f + enemy.phase) * 48.0f;
			targetY = enemy.homeY + std::cos(m_Time * 0.3f + enemy.phase) * 16.0f;
		}

		const float dx = targetX - enemy.x;
		const float dy = targetY - enemy.y;
		const float length = std::sqrt(dx * dx + dy * dy);

		float speed = type.speed;
		if (warden && !enemy.engaged)
		{
			speed *= 0.45f;
		}
		if (!warden && TotalBrightness(enemy.x, enemy.y) >= Companion::SafeBrightness())
		{
			speed *= kLightSlow;
		}

		float moveX = 0.0f;
		float moveY = 0.0f;
		const float stopAt = (warden && !enemy.engaged) ? 2.0f : type.radius + kPlayerRadius * 0.6f;
		if (length > stopAt)
		{
			moveX = dx / length * speed * deltaSeconds;
			moveY = dy / length * speed * deltaSeconds;
		}

		// 서로 겹치지 않게 밀어낸다. 무리가 한 점으로 뭉치면 한 마리처럼 보인다.
		for (size_t j = 0; j < m_Enemies.size(); ++j)
		{
			if (j == i || m_Enemies[j].dying > 0.0f)
			{
				continue;
			}
			const float ox = enemy.x - m_Enemies[j].x;
			const float oy = enemy.y - m_Enemies[j].y;
			const float gap = std::sqrt(ox * ox + oy * oy);
			const float minimum = type.radius + EnemyTypeOf(m_Enemies[j].kind).radius;
			if (gap > 0.01f && gap < minimum)
			{
				const float push = (minimum - gap) * 4.0f * deltaSeconds;
				moveX += ox / gap * push;
				moveY += oy / gap * push;
			}
		}

		// 불씨에 맞아 밀려난 만큼
		moveX += enemy.knockX * deltaSeconds;
		moveY += enemy.knockY * deltaSeconds;
		const float damping = std::exp(-8.0f * deltaSeconds);
		enemy.knockX *= damping;
		enemy.knockY *= damping;

		TryMoveBody(enemy.x, enemy.y, moveX, moveY, type.radius, !warden);

		// ── 붙으면 할퀸다 ──
		if ((!warden || enemy.engaged) && enemy.touchTimer <= 0.0f
		 && Distance(enemy.x, enemy.y, m_PlayerX, m_PlayerY) < type.radius + kPlayerRadius)
		{
			DamagePlayer(type.touchDamage * (1.0f + 0.5f * (enemy.power - 1.0f)), enemy.x, enemy.y);
			enemy.touchTimer = type.touchCooldown;
		}

		// ── 종탑지기: 어둠 구슬을 사방으로 흩뿌린다 ──
		// 집 벽에 막히므로, 집 뒤로 숨는 것이 피하는 방법이 된다.
		if (warden && enemy.engaged)
		{
			enemy.burstTimer -= deltaSeconds;
			if (enemy.burstTimer <= 0.0f)
			{
				const float offset = m_Time * 0.7f;
				for (int k = 0; k < kWardenOrbs; ++k)
				{
					const float angle = offset + k * kTwoPi / kWardenOrbs;
					Orb orb;
					orb.x = enemy.x;
					orb.y = enemy.y - 34.0f;
					orb.vx = std::cos(angle) * kOrbSpeed;
					orb.vy = std::sin(angle) * kOrbSpeed;
					orb.damage = 12.0f * (1.0f + 0.5f * (enemy.power - 1.0f));
					orb.life = kOrbLife;
					m_Orbs.push_back(orb);
				}
				enemy.burstTimer = kWardenBurstEvery;
			}
		}
	}
}

void Game::FireAtEnemies(bool attacking)
{
	if (!attacking || m_Combat.attackCooldown > 0.0f)
	{
		return;
	}

	const int level = m_Combat.level;
	const int tier = m_Combat.weaponTier;
	const float range = Progression::AttackRange(level);

	// 사거리 안의 적을 가까운 순서로
	std::vector<std::pair<float, int>> targets;
	for (const Enemy& enemy : m_Enemies)
	{
		if (enemy.dying > 0.0f)
		{
			continue;
		}
		const float distance = Distance(m_PlayerX, m_PlayerY, enemy.x, BodyCenterY(enemy));
		if (distance <= range)
		{
			targets.push_back(std::make_pair(distance, enemy.id));
		}
	}
	std::sort(targets.begin(), targets.end());

	const int shots = Progression::ProjectileCount(level);
	const float speed = Progression::ProjectileSpeed(tier);

	// 손에 든 등불에서 불씨가 떠난다
	const float originX = m_PlayerX + m_FacingX * 10.0f;
	const float originY = m_PlayerY - 10.0f;

	for (int s = 0; s < shots; ++s)
	{
		Projectile projectile;
		projectile.x = originX;
		projectile.y = originY;
		projectile.damage = Progression::AttackDamage(level, tier);
		projectile.pierce = Progression::ProjectilePierce(tier);
		projectile.travel = range * 1.15f;

		float directionX = m_FacingX;
		float directionY = 0.0f;

		if (!targets.empty())
		{
			// 과녁이 여럿이면 하나씩 나눠 맡고, 모자라면 같은 과녁에 부채꼴로 쏜다.
			const Enemy* target = FindEnemy(targets[s % targets.size()].second);
			if (target != NULL)
			{
				directionX = target->x - originX;
				directionY = BodyCenterY(*target) - originY;
				projectile.targetId = target->id;
			}
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

		projectile.vx = directionX * speed;
		projectile.vy = directionY * speed;
		m_Projectiles.push_back(projectile);
	}

	m_Combat.attackCooldown = Progression::AttackCooldown(level, tier);
}

void Game::UpdateProjectiles(float deltaSeconds)
{
	for (Projectile& projectile : m_Projectiles)
	{
		const float speed = std::sqrt(projectile.vx * projectile.vx + projectile.vy * projectile.vy);

		// 쫓던 적이 살아 있으면 조금씩 방향을 틀어 따라간다
		if (projectile.targetId >= 0)
		{
			const Enemy* target = FindEnemy(projectile.targetId);
			if (target != NULL && target->dying == 0.0f && speed > 0.0f)
			{
				float wantX = target->x - projectile.x;
				float wantY = BodyCenterY(*target) - projectile.y;
				const float wantLength = std::sqrt(wantX * wantX + wantY * wantY);
				if (wantLength > 0.001f)
				{
					const float turn = std::min(1.0f, 9.0f * deltaSeconds);
					float newX = projectile.vx / speed + (wantX / wantLength - projectile.vx / speed) * turn;
					float newY = projectile.vy / speed + (wantY / wantLength - projectile.vy / speed) * turn;
					const float newLength = std::sqrt(newX * newX + newY * newY);
					if (newLength > 0.001f)
					{
						projectile.vx = newX / newLength * speed;
						projectile.vy = newY / newLength * speed;
					}
				}
			}
			else
			{
				projectile.targetId = -1;
			}
		}

		projectile.x += projectile.vx * deltaSeconds;
		projectile.y += projectile.vy * deltaSeconds;
		projectile.travel -= speed * deltaSeconds;

		for (Enemy& enemy : m_Enemies)
		{
			if (enemy.dying > 0.0f || enemy.id == projectile.lastHitId)
			{
				continue;
			}

			const float reach = EnemyTypeOf(enemy.kind).radius + 4.0f;
			if (Distance(projectile.x, projectile.y, enemy.x, BodyCenterY(enemy)) < reach)
			{
				DamageEnemy(enemy, projectile.damage, projectile.x, projectile.y);
				projectile.lastHitId = enemy.id;
				projectile.targetId = -1;
				if (--projectile.pierce < 0)
				{
					projectile.travel = 0.0f;
				}
				break;
			}
		}
	}

	m_Projectiles.erase(std::remove_if(m_Projectiles.begin(), m_Projectiles.end(),
	                                   [](const Projectile& p) { return p.travel <= 0.0f; }),
	                    m_Projectiles.end());
}

void Game::UpdateOrbs(float deltaSeconds)
{
	for (Orb& orb : m_Orbs)
	{
		orb.life -= deltaSeconds;
		orb.x += orb.vx * deltaSeconds;
		orb.y += orb.vy * deltaSeconds;

		if (IsBlockedAtWorld(orb.x, orb.y))
		{
			orb.life = 0.0f;   // 집 벽과 나무에 막힌다
			continue;
		}

		if (Distance(orb.x, orb.y, m_PlayerX, m_PlayerY - 14.0f) < 6.0f + kPlayerRadius)
		{
			DamagePlayer(orb.damage, orb.x, orb.y);
			orb.life = 0.0f;
		}
	}

	m_Orbs.erase(std::remove_if(m_Orbs.begin(), m_Orbs.end(),
	                            [](const Orb& o) { return o.life <= 0.0f; }),
	             m_Orbs.end());
}

void Game::DamageEnemy(Enemy& enemy, float damage, float fromX, float fromY)
{
	if (enemy.dying > 0.0f)
	{
		return;
	}

	const bool warden = (enemy.kind == EnemyKind::Warden);
	const float radius = EnemyTypeOf(enemy.kind).radius;

	enemy.health -= damage;
	enemy.hitFlash = 0.12f;

	// 맞은 반대쪽으로 밀려난다. 종탑지기는 거의 밀리지 않는다.
	float awayX = enemy.x - fromX;
	float awayY = BodyCenterY(enemy) - fromY;
	const float length = std::sqrt(awayX * awayX + awayY * awayY);
	if (length > 0.001f)
	{
		const float strength = warden ? 40.0f : 260.0f;
		enemy.knockX += awayX / length * strength;
		enemy.knockY += awayY / length * strength;
	}

	AddFloatingText(enemy.x, enemy.y - radius * 2.0f - 8.0f, std::string(),
	                static_cast<int>(damage + 0.5f), kColorDamage, warden ? 17.0f : 15.0f);

	// 멀리서 쏴도 종탑지기는 깨어난다
	if (warden && !enemy.engaged)
	{
		enemy.engaged = true;
		enemy.burstTimer = 1.2f;
	}

	if (enemy.health <= 0.0f)
	{
		KillEnemy(enemy);
	}
}

void Game::KillEnemy(Enemy& enemy)
{
	enemy.health = 0.0f;
	enemy.dying = 0.001f;
	DropLoot(enemy);

	if (enemy.kind == EnemyKind::Warden)
	{
		m_Regions.MarkWardenDefeated(enemy.regionX, enemy.regionY);
		m_Orbs.clear();   // 흩뿌린 구슬도 함께 흩어진다
		RefreshNearbyRegions();
		AddFloatingText(enemy.x, enemy.y - 90.0f, "종탑지기가 쓰러졌다", -1, kColorGold, 20.0f);
	}
}

void Game::DropLoot(const Enemy& enemy)
{
	const EnemyType& type = EnemyTypeOf(enemy.kind);
	const bool warden = (enemy.kind == EnemyKind::Warden);

	auto drop = [&](PickupKind kind, int value)
	{
		Pickup pickup;
		pickup.kind = kind;
		pickup.value = value;
		pickup.x = enemy.x;
		pickup.y = enemy.y - 4.0f;
		const float angle = RandomRange(0.0f, kTwoPi);
		const float speed = RandomRange(60.0f, warden ? 260.0f : 150.0f);
		pickup.vx = std::cos(angle) * speed;
		pickup.vy = std::sin(angle) * speed;
		m_Pickups.push_back(pickup);
	};

	// 섬 안쪽의 적일수록 영혼이 짙다
	const int soulValue = std::max(1, static_cast<int>(type.soulValue * (0.5f + 0.5f * enemy.power) + 0.5f));
	for (int i = 0; i < type.souls; ++i)
	{
		drop(PickupKind::Soul, soulValue);
	}

	if (Roll(type.healChance))
	{
		drop(PickupKind::Heal, 1);
	}

	if (Roll(type.weaponChance))
	{
		// 무기가 다 강해졌으면 강화석 대신 짙은 영혼석을 준다
		if (m_Combat.weaponTier < Progression::kMaxWeaponTier)
		{
			drop(PickupKind::Weapon, 1);
		}
		else
		{
			drop(PickupKind::Soul, soulValue * 3);
		}
	}

	if (Roll(type.magnetChance))
	{
		drop(PickupKind::Magnet, 1);
	}
}

void Game::UpdatePickups(float deltaSeconds)
{
	const bool magnet = m_Combat.magnetTime > 0.0f;
	const float attract = magnet ? kMagnetRadius : Progression::PickupRadius(m_Combat.level);

	for (Pickup& pickup : m_Pickups)
	{
		pickup.age += deltaSeconds;

		// 흩어지며 떨어졌다가 멈춘다
		const float damping = std::exp(-5.0f * deltaSeconds);
		pickup.vx *= damping;
		pickup.vy *= damping;
		pickup.x += pickup.vx * deltaSeconds;
		pickup.y += pickup.vy * deltaSeconds;

		const float distance = Distance(pickup.x, pickup.y, m_PlayerX, m_PlayerY);

		// 가까우면 저절로 끌려온다. 자석이 켜져 있으면 멀리 있는 것까지 전부.
		if (!m_PendingRespawn && pickup.age > kPickupSettle && distance < attract && distance > 0.001f)
		{
			const float pull = magnet ? 560.0f : 260.0f + (attract - distance) * 2.0f;
			const float step = std::min(distance, pull * deltaSeconds);
			pickup.x += (m_PlayerX - pickup.x) / distance * step;
			pickup.y += (m_PlayerY - pickup.y) / distance * step;
		}

		if (!m_PendingRespawn && Distance(pickup.x, pickup.y, m_PlayerX, m_PlayerY) < kPickupReach)
		{
			CollectPickup(pickup);
			pickup.age = kPickupLifetime + 1.0f;   // 치운다
		}
	}

	m_Pickups.erase(std::remove_if(m_Pickups.begin(), m_Pickups.end(),
	                               [](const Pickup& p) { return p.age > kPickupLifetime; }),
	                m_Pickups.end());
}

void Game::CollectPickup(const Pickup& pickup)
{
	const float headY = m_PlayerY - 44.0f;

	switch (pickup.kind)
	{
	case PickupKind::Soul:
		AddFloatingText(m_PlayerX, headY, std::string(), pickup.value, kColorSoul, 13.0f);
		GainSouls(pickup.value);
		break;

	case PickupKind::Heal:
	{
		const float maxHealth = Progression::MaxHealth(m_Combat.level);
		m_Combat.health = std::min(maxHealth, m_Combat.health + maxHealth * 0.3f);
		AddFloatingText(m_PlayerX, headY, "체력 회복", -1, kColorHeal, 15.0f);
		break;
	}

	case PickupKind::Weapon:
		if (m_Combat.weaponTier < Progression::kMaxWeaponTier)
		{
			++m_Combat.weaponTier;
			AddFloatingText(m_PlayerX, headY, "무기 강화", -1, kColorGold, 16.0f);
		}
		else
		{
			GainSouls(5);
		}
		break;

	case PickupKind::Magnet:
		m_Combat.magnetTime = kMagnetSeconds;
		AddFloatingText(m_PlayerX, headY, "자석", -1, kColorSoul, 15.0f);
		break;
	}
}

void Game::GainSouls(int amount)
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
		AddFloatingText(m_PlayerX, m_PlayerY - 60.0f, "레벨 업!", -1, kColorGold, 20.0f);
	}
}

void Game::DamagePlayer(float amount, float fromX, float fromY)
{
	if (m_Combat.invulnerable > 0.0f || m_PendingRespawn)
	{
		return;
	}

	m_Combat.health -= amount;
	m_Combat.invulnerable = kInvulnerableTime;
	m_Combat.hurtFlash = 1.0f;

	AddFloatingText(m_PlayerX, m_PlayerY - 44.0f, std::string(),
	                static_cast<int>(amount + 0.5f), kColorHurt, 15.0f);

	// 맞은 반대쪽으로 조금 밀려난다. 벽은 뚫지 않는다.
	float awayX = m_PlayerX - fromX;
	float awayY = m_PlayerY - fromY;
	const float length = std::sqrt(awayX * awayX + awayY * awayY);
	if (length > 0.001f)
	{
		const float pushX = awayX / length * 14.0f;
		const float pushY = awayY / length * 14.0f;
		if (!IsPlayerBlocked(m_PlayerX + pushX, m_PlayerY)) { m_PlayerX += pushX; }
		if (!IsPlayerBlocked(m_PlayerX, m_PlayerY + pushY)) { m_PlayerY += pushY; }
	}

	if (m_Combat.health <= 0.0f)
	{
		m_Combat.health = 0.0f;
		m_PendingRespawn = true;
		m_RespawnTimer = kRespawnDelay;
		m_Combat.attackCooldown = kRespawnDelay;
	}
}

void Game::RespawnPlayer()
{
	// 쓰러지면 가장 가까운 되찾은 종탑 앞, 없으면 처음 자리에서 다시 일어선다.
	float bestX = m_StartX;
	float bestY = m_StartY;
	float best = Distance(m_PlayerX, m_PlayerY, m_StartX, m_StartY);

	std::vector<Regions::Region> rung;
	m_Regions.CollectRung(rung);
	for (const Regions::Region& region : rung)
	{
		const float x = TileToWorld(region.towerTileX);
		const float y = TileToWorld(region.towerTileY) + 64.0f;
		const float distance = Distance(m_PlayerX, m_PlayerY, x, y);
		if (distance < best)
		{
			best = distance;
			bestX = x;
			bestY = y;
		}
	}

	m_PlayerX = bestX;
	m_PlayerY = bestY;
	m_CameraX = bestX;
	m_CameraY = bestY;

	m_PlayerPath.clear();
	m_PathLength = 0.0f;
	m_PlayerPath.push_back({ m_PlayerX, m_PlayerY, 0.0f });

	// 동행자도 곁으로 데려온다
	if (m_Companion.IsAwake())
	{
		m_Companion.Wake(m_PlayerX - 24.0f, m_PlayerY, m_Companion.RegionX(), m_Companion.RegionY());
	}

	m_Combat.health = Progression::MaxHealth(m_Combat.level);
	m_Combat.healthShown = m_Combat.health;
	m_Combat.invulnerable = 2.0f;
	m_Combat.hurtFlash = 0.0f;
	m_Oil = std::max(m_Oil, 0.5f);

	m_Enemies.clear();
	m_Orbs.clear();
	m_Projectiles.clear();
	m_SpawnTimer = 4.0f;

	m_PendingRespawn = false;
	++m_Combat.deaths;

	RefreshNearbyRegions();
	AddFloatingText(m_PlayerX, m_PlayerY - 60.0f, "다시 일어섰다", -1, kColorGold, 18.0f);
}
