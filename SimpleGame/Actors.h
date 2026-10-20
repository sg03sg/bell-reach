#pragma once

#include <deque>
#include <string>
#include <vector>

#include "Actor.h"
#include "Combat.h"
#include "Regions.h"
#include "World.h"

//
// 화면에 놓이는 것들.
//
// 모두 Actor를 물려받아 자기 자리와 자기 행동과 자기 모습만 안다.
// 순서는 SceneGraph가 맡고, 모습은 Art의 도안을 빌려 그린다.
// 세계에 관한 질문(밝기, 지형, 구역 상태)은 ActorContext의 Game에게 묻는다.
//
// 놓이는 방식은 두 가지다.
//   · 지형에서 나온 것(땅, 집, 나무, 물건, 종탑, 굳은 사람)은 Game이 청크 단위로
//     놓고 치운다. 플레이어에게서 멀어지면 사라지고 다시 오면 되살아난다.
//   · 살아 움직이는 것(플레이어, 동행자, 적, 불씨, 떨어진 물건)은 스스로 생겼다
//     스스로 Destroy()를 부른다.
//

// ── 땅 ───────────────────────────────────────────────────────────
// 타일과 조명을 그리는 단 하나의 액터. 다른 모든 것 아래에 깔린다.
class GroundActor : public Actor
{
public:
	GroundActor();
	void OnDraw(const DrawContext& context) const override;
	float SortDepth() const override { return -1000000.0f; }
};

// ── 지형에서 나온 것들 ───────────────────────────────────────────
class HouseActor : public Actor
{
public:
	explicit HouseActor(const World::House& house);
	void OnDraw(const DrawContext& context) const override;
private:
	World::House m_House;
};

class TreeActor : public Actor
{
public:
	explicit TreeActor(const World::Tree& tree);
	void OnDraw(const DrawContext& context) const override;
private:
	World::Tree m_Tree;
};

class WallActor : public Actor
{
public:
	explicit WallActor(const World::Wall& wall);
	void OnDraw(const DrawContext& context) const override;
private:
	World::Wall m_Wall;
};

class PropActor : public Actor
{
public:
	explicit PropActor(const World::Prop& prop);
	void OnDraw(const DrawContext& context) const override;
private:
	World::Prop m_Prop;
};

// 종탑. 구역의 상태(울렸는가)는 Regions가 들고 있으므로 매 프레임 물어본다.
class TowerActor : public Actor
{
public:
	// 자리는 Game이 구역에서 계산해 넘긴다. 액터가 씨앗을 다시 들출 필요가 없다.
	TowerActor(int regionX, int regionY, float worldX, float worldY);
	void OnDraw(const DrawContext& context) const override;
	int RegionX() const { return m_RegionX; }
	int RegionY() const { return m_RegionY; }
private:
	int m_RegionX;
	int m_RegionY;
};

// 굳어 있는 사람. 깨어나 따라나서면 스스로 사라지고 CompanionActor가 대신 선다.
class SleeperActor : public Actor
{
public:
	SleeperActor(int regionX, int regionY, float worldX, float worldY);
	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
private:
	int m_RegionX;
	int m_RegionY;
};

// 내려놓은 화톳불. 스스로 빛을 낸다.
class FireActor : public Actor
{
public:
	FireActor(float worldX, float worldY, float radius);
	void OnDraw(const DrawContext& context) const override;
	bool GetLight(float& radius, bool& leavesTrail) const override;
private:
	float m_Radius;
	float m_Seed;
};

// ── 사람 ─────────────────────────────────────────────────────────
class PlayerActor : public Actor
{
public:
	// 플레이어가 지나온 자취의 한 점. 동행자가 이 위를 따라 걷는다.
	struct PathPoint
	{
		float x;
		float y;
		float travelled;
	};

	PlayerActor(float worldX, float worldY);

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
	bool GetLight(float& radius, bool& leavesTrail) const override;

	// ── 상태 ──
	const PlayerCombat& Combat() const { return m_Combat; }
	PlayerCombat& Combat() { return m_Combat; }
	float Oil() const { return m_Oil; }
	void SetOil(float oil) { m_Oil = oil; }
	float Facing() const { return m_Facing; }
	void SetFacing(float facing) { m_Facing = facing; }
	bool IsMoving() const { return m_Moving; }
	void SetMoving(bool moving) { m_Moving = moving; }
	int FiresLeft() const { return m_FiresLeft; }
	void UseFire() { --m_FiresLeft; }

	// ── 싸움 ──
	void TakeDamage(const ActorContext& context, float amount, float fromX, float fromY);
	void GainSouls(const ActorContext& context, int amount);
	void Heal(float amount);
	void AddWeaponTier();
	void StartMagnet(float seconds);
	float MaxHealth() const;

	// 쓰러진 자리에서 다시 일어설 때 Game이 부른다.
	void PlaceAt(float worldX, float worldY);

	// ── 자취 ──
	// 뒤로 lag 픽셀만큼 떨어진 지점. 동행자가 따라갈 목표다.
	bool TrailPoint(float lag, float& outX, float& outY) const;

private:
	void Walk(const ActorContext& context);
	void RecordTrail();
	void Fire(const ActorContext& context);

	PlayerCombat m_Combat;
	float m_Oil = 1.0f;
	int m_FiresLeft = 3;
	float m_Facing = 1.0f;
	bool m_Moving = false;

	std::deque<PathPoint> m_Path;
	float m_PathLength = 0.0f;
};

// 따라나선 사람. 길찾기를 하지 않고 플레이어의 자취를 밟는다.
class CompanionActor : public Actor
{
public:
	CompanionActor(float worldX, float worldY, int regionX, int regionY);

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

	float Darkness() const { return m_Darkness; }
	bool IsFrozenAgain() const { return m_Darkness >= 1.0f; }
	int RegionX() const { return m_RegionX; }
	int RegionY() const { return m_RegionY; }

	// 어둠 속에서 완전히 굳기까지, 빛 속에서 온전히 돌아오기까지.
	static float ErodeSeconds() { return 40.0f; }
	static float RecoverSeconds() { return 8.0f; }

private:
	float m_Darkness = 0.0f;
	int m_RegionX;
	int m_RegionY;
	bool m_Walking = false;
};

// ── 싸움 ─────────────────────────────────────────────────────────
class EnemyActor : public Actor
{
public:
	EnemyActor(EnemyKind kind, float worldX, float worldY, float power);
	~EnemyActor() override;

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

	const Enemy& Data() const { return m_Data; }
	int Id() const { return m_Data.id; }
	bool IsDying() const { return m_Data.dying > 0.0f; }
	float Radius() const;
	float BodyCenterY() const;          // 발밑이 아니라 몸 가운데. 과녁이 되는 자리다
	bool IsWarden() const { return m_Data.kind == EnemyKind::Warden; }
	bool IsEngaged() const { return m_Data.engaged; }
	float Health() const { return m_Data.health; }
	float MaxHealth() const { return m_Data.maxHealth; }

	void TakeDamage(const ActorContext& context, float damage, float fromX, float fromY);
	void MakeWarden(int regionX, int regionY, float homeX, float homeY);

	// Game이 적 목록을 들고 있으므로, 태어나고 사라질 때 알려 준다.
	void AttachToGame(Game* game);

private:
	void Die(const ActorContext& context);
	void DropLoot(const ActorContext& context);

	Enemy m_Data;
	Game* m_Game = NULL;
	float m_Facing = 1.0f;
};

// 등불에서 날아간 불씨
class ProjectileActor : public Actor
{
public:
	ProjectileActor(float worldX, float worldY, float velocityX, float velocityY,
	                float damage, int pierce, float travel, int targetId);
	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
private:
	Projectile m_Data;
};

// 종탑지기가 흩뿌린 어둠 구슬
class OrbActor : public Actor
{
public:
	OrbActor(float worldX, float worldY, float velocityX, float velocityY, float damage, float life);
	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
private:
	Orb m_Data;
	float m_Seed;
};

// 떨어진 물건
class PickupActor : public Actor
{
public:
	PickupActor(PickupKind kind, float worldX, float worldY, float velocityX, float velocityY, int value);
	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
private:
	void Collect(const ActorContext& context);

	Pickup m_Data;
	float m_Seed;
};

// 머리 위로 떠올랐다 사라지는 글자. 화면 맨 위 층에 그려진다.
class FloatingTextActor : public Actor
{
public:
	FloatingTextActor(float worldX, float worldY, const std::string& label, int number,
	                  const float color[3], float size);
	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;
private:
	FloatingText m_Data;
};
