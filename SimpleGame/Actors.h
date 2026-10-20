#pragma once

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "Actor.h"
#include "BehaviorTree.h"
#include "Perks.h"
#include "Professions.h"
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

// 상인에게 사서 세운 횃불. 꺼지지 않는다.
// 화톳불보다 작게 비추지만 한 번 세우면 그 자리가 계속 밝다 — 지나온 숲길에
// 하나씩 세워 두면 돌아오는 길이 된다.
class TorchActor : public Actor
{
public:
	TorchActor(float worldX, float worldY);
	void OnDraw(const DrawContext& context) const override;
	bool GetLight(float& radius, bool& leavesTrail) const override;
private:
	float m_Seed;
};

// 마을 사람이 제 집 둘레에 놓은 장식. 돈이 모일 때마다 하나씩 는다.
// 등 기둥은 스스로 빛나서, 마을이 부유해질수록 밤이 밝아진다.
class DecorActor : public Actor
{
public:
	DecorActor(Decor kind, float worldX, float worldY, const float tint[3], unsigned int seed);
	void OnDraw(const DrawContext& context) const override;
	bool GetLight(float& radius, bool& leavesTrail) const override;
private:
	Decor m_Kind;
	float m_Tint[3];
	unsigned int m_Seed;
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
	// 잠깐 아무것도 받지 않는다. 수도승의 기도가 씌운다.
	void Shield(float seconds);

	// ── 마을에서 쓰는 것 ──
	void AddGold(int amount) { m_Combat.gold += amount; }
	bool SpendGold(int amount);          // 모자라면 쓰지 않고 false
	void AddTorch() { ++m_Combat.torches; }
	bool UseTorch();
	void AddHealPack() { ++m_Combat.healPacks; }
	bool UseHealPack();
	void Refuel(float amount) { m_Oil = std::min(1.0f, m_Oil + amount); }
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
	// 되찾은 사람들이 주는 보정. 매 프레임 Game에서 베껴 둔다.
	// 능력치를 재는 곳마다 Game을 거치지 않아도 되도록.
	Perks::Modifiers m_Mods;

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
	// profession은 직업 번호(Skills.h). 대사·특성과 같은 번호다.
	CompanionActor(float worldX, float worldY, int regionX, int regionY, int profession);

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

	// ── 함께 싸우기 ──
	int Profession() const { return m_Profession; }
	float SkillCooldown() const { return m_SkillTimer; }   // 0이면 쓸 수 있다
	float SkillCastGlow() const { return m_CastGlow; }

	float Darkness() const { return m_Darkness; }
	bool IsFrozenAgain() const { return m_Darkness >= 1.0f; }
	int RegionX() const { return m_RegionX; }
	int RegionY() const { return m_RegionY; }

	// 어둠 속에서 완전히 굳기까지, 빛 속에서 온전히 돌아오기까지.
	static float ErodeSeconds() { return 40.0f; }
	static float RecoverSeconds() { return 8.0f; }

private:
	// 쓸 수 있으면 쓰고 true. 조건이 안 맞으면 아무것도 안 하고 false.
	bool UseSkill(const ActorContext& context);

	float m_Darkness = 0.0f;
	int m_RegionX;
	int m_RegionY;
	bool m_Walking = false;

	int m_Profession;
	float m_SkillTimer = 1.5f;   // 깨어나자마자 쓰지는 않는다
	float m_CastGlow = 0.0f;     // 막 썼을 때 몸이 잠깐 빛난다
	float m_Facing = 1.0f;       // 스킬을 쓴 쪽을 본다
	float m_FacingHold = 0.0f;   // 그쪽을 보고 있을 남은 시간
};

//
// 첫 마을에 사는 사람.
//
// 되찾은 구역과 달리 첫 마을은 처음부터 밝다. 여기 사람들은 굳어 있지
// 않고 제 일을 하며 돌아다닌다. 이 게임에서 "아직 잃지 않은 것"이
// 어떤 모습인지 보여 주는 자리다.
//
// 저마다 직업과 지갑이 있다(Professions.h). 사냥꾼은 문 밖 어둠을 잡아
// 현상금을 벌고, 사람들은 그 돈으로 장을 보고 치료를 받고, 돈이 모이면
// 제 집 둘레를 꾸민다. 가난한 좀도둑은 부자를 노리고, 들키면 시비가 붙는다.
//
// 무엇을 할지는 행동 트리가 정한다(BehaviorTree.h). 다음에 할 일(심부름)을
// 고르는 것은 직업과 지갑과 몸 상태에 달려 있다(PickErrand).
//
class VillagerActor : public Actor
{
public:
	// 다음에 할 일. 목적지에 닿으면 이 일을 한다.
	enum class Errand
	{
		None,
		Stroll,      // 집 둘레를 거닌다
		Patrol,      // 울타리 안쪽을 돈다
		Hunt,        // 문으로 나가 어둠을 기다린다(사냥꾼)
		Tend,        // 가게를 지킨다 / 집에서 일한다
		Decorate,    // 집 둘레를 꾸미러 간다
		Shop,        // 상인에게 장을 보러 간다
		Doctor,      // 의사에게 치료받으러 간다
		Steal        // 부자의 주머니를 노리고 몰래 다가간다(좀도둑)
	};

	// look은 겉모습을 정하는 값이고, perkIndex가 0 이상이면
	// 되찾아 와서 여기 자리 잡은 사람이다(그 직업 차림으로 서 있는다).
	VillagerActor(float worldX, float worldY, unsigned int look, Profession profession,
	              int perkIndex = -1);
	~VillagerActor() override;

	// 스폰한 뒤에 불러 준다. 번호를 받고 과녁 목록에 들어간다.
	// villageX/Y는 마을 한가운데(종탑). 순찰과 싸움이 여기서 너무 멀어지지 않는다.
	void AttachToGame(Game* game, float villageX, float villageY);

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

	// ── 누구인가 ──
	int Id() const { return m_Id; }
	Profession GetProfession() const { return m_Profession; }
	std::string DisplayName() const;     // "상인", "의사" … 되찾아 온 사람은 그 직업
	bool IsThief() const { return m_Thief; }
	void MakeThief() { m_Thief = true; }
	float HomeX() const { return m_Board.homeX; }
	float HomeY() const { return m_Board.homeY; }

	// ── 지갑 ──
	int Gold() const { return m_Gold; }
	void AddGold(int amount);
	int TakeGold(int amount);            // 가진 만큼만 빠진다. 실제로 빠진 양을 돌려준다
	int Decorations() const { return m_Decorations; }

	// ── 복수(플레이어에게) ──
	// 누가 먼저 때리면 원한을 품는다. 둘레의 사람들도 함께 품는다.
	void Provoke(float seconds);
	bool HasGrudge() const { return m_Board.grudge > 0.0f; }
	float Grudge() const { return m_Board.grudge; }
	bool IsPatrolling() const { return m_Board.patrolling; }

	// ── 다툼(마을 사람끼리) ──
	// 도둑맞은 사람과 경비가 도둑을 쫓는다. 도둑은 달아나다가 한 대 맞으면
	// 돌아서서 맞서고, 두 대째에 훔친 돈을 내놓고 항복한다.
	void StartQuarrel(int quarryId, float seconds);
	void EndQuarrel();
	bool InQuarrel() const { return m_Board.quarrel > 0.0f; }
	int Quarry() const { return m_Board.quarry; }
	bool IsFleeing() const { return m_Board.fleeing; }
	int Stolen() const { return m_Stolen; }

	// ── 맞는 것 ──
	// 플레이어의 불씨는 사람을 죽일 수 있다. 마을 사람끼리 던진 돌은 아프기만 하다.
	void TakeDamage(const ActorContext& context, float amount);
	void TakeHitFrom(const ActorContext& context, float amount, int attackerId);
	bool IsDead() const { return m_Health <= 0.0f; }
	float Health() const { return m_Health; }
	float MaxHealth() const { return 60.0f; }
	void HealFully() { m_Health = MaxHealth(); }

	float Radius() const { return 11.0f; }
	float BodyCenterY() const { return WorldY() - 16.0f; }

	// 되찾아 온 사람인가. 그렇다면 어느 특성을 주는 사람인가
	int PerkIndex() const { return m_PerkIndex; }
	bool IsRescued() const { return m_PerkIndex >= 0; }

	// 지금 무엇을 하는 중인지. 머리 위에 띄우거나 로그로 본다
	const char* Activity() const { return m_Board.activity; }

	// ── 행동 트리의 마디들이 부른다 ──
	void PickErrand(const ActorContext& context);   // 다음에 할 일을 고른다
	void DoErrand(const ActorContext& context);     // 목적지에 닿았다. 그 일을 한다
	void Surrender(const ActorContext& context);    // 훔친 돈을 내놓는다

private:
	void BuildTree();
	void SetErrand(Errand errand, float targetX, float targetY, int with = -1);
	void FollowErrandTarget(const ActorContext& context);   // 움직이는 상대를 따라 목적지를 고친다
	void TrySteal(const ActorContext& context);

	BT::NodePtr m_Tree;
	BT::Blackboard m_Board;

	Game* m_Game = NULL;
	int m_Id = -1;
	float m_Health = 60.0f;

	Profession m_Profession;
	bool m_Thief = false;
	int m_Gold = 0;
	int m_Decorations = 0;
	float m_WageTimer = 12.0f;   // 다음 품삯까지

	// 도둑질
	int m_Stolen = 0;            // 지금 들고 달아나는 돈
	int m_StolenFrom = -1;       // 누구의 돈인가
	int m_HitsTaken = 0;         // 쫓기며 맞은 횟수

	unsigned int m_Look;
	int m_PerkIndex;
	float m_Facing = 1.0f;
	bool m_Walking = false;
	float m_Bob = 0.0f;       // 걸을 때 위아래로 흔들리는 위상
};

//
// 사람이 던지거나 쏘는 것 — 돌, 화살, 폭약.
//
// 플레이어의 불씨(ProjectileActor)와 따로 둔 것은 편이 있기 때문이다.
// 마을 사람의 돌은 적을 노릴 수도, 원한을 품으면 플레이어를 노릴 수도 있다.
// 어느 쪽이든 같은 편은 맞히지 않는다.
//
class ThrownActor : public Actor
{
public:
	enum class Kind { Stone, Arrow, Bomb };
	enum class Team { HitsEnemies, HitsPlayer, HitsVillager };

	// 과녁 자리를 향해 날아간다. 돌과 화살은 지나쳐도 사거리까지 계속 가고,
	// 폭약은 과녁 자리에 떨어져 터진다.
	ThrownActor(Kind kind, Team team, float worldX, float worldY,
	            float targetX, float targetY, float speed, float damage, float range);

	// 폭약이 터지는 반경
	void SetBlast(float radius) { m_Blast = radius; }

	// 던진 마을 사람. 적이 이 돌에 쓰러지면 그 사람이 현상금을 받는다
	void SetOwner(int villagerId) { m_Owner = villagerId; }

	// HitsVillager일 때 맞힐 마을 사람. 다른 사람은 그냥 지나친다
	void SetTarget(int villagerId) { m_Target = villagerId; }

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

private:
	void Explode(const ActorContext& context);

	Kind m_Kind;
	Team m_Team;
	float m_VX;
	float m_VY;
	float m_Damage;
	float m_Range;          // 날아갈 거리
	float m_Flown = 0.0f;   // 날아온 거리
	float m_Blast = 0.0f;
	float m_Spin = 0.0f;
	int m_Owner = -1;
	int m_Target = -1;
};

//
// 보이기만 하는 것 — 퍼지는 고리, 베는 궤적, 폭발, 기도의 빛.
// 피해나 효과는 스킬을 쓴 쪽이 이미 적용했다. 이건 그 순간을 보여 줄 뿐이다.
//
class EffectActor : public Actor
{
public:
	enum class Kind { Shockwave, BellRing, Slash, Blast, Blessing };

	EffectActor(Kind kind, float worldX, float worldY, float radius, float life, const float tint[3]);

	// 베기의 방향(라디안). 다른 효과는 쓰지 않는다
	void SetAngle(float angle) { m_Angle = angle; }

	// 다른 액터를 따라다니게 한다(기도의 빛이 플레이어를 따라가도록)
	void Follow(const Actor* target) { m_Follow = target; }

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

private:
	Kind m_Kind;
	float m_Radius;
	float m_Life;
	float m_Age = 0.0f;
	float m_Angle = 0.0f;
	float m_Tint[3];
	const Actor* m_Follow = NULL;
};

//
// 땅에 깔리는 것 — 독 연기, 봉인의 글.
// 깔린 동안 안에 든 적에게 영향을 준다. 피해는 매 프레임이 아니라
// 0.5초마다 나눠 준다(피해 숫자가 화면을 덮지 않게).
//
class ZoneActor : public Actor
{
public:
	enum class Kind { Poison, Seal };

	ZoneActor(Kind kind, float worldX, float worldY, float radius, float life,
	          float damagePerSecond, float slowFactor);

	void OnUpdate(const ActorContext& context) override;
	void OnDraw(const DrawContext& context) const override;

	// 땅에 깔리므로 서 있는 것들보다 먼저 그린다
	float SortDepth() const override { return -1.0e9f; }

private:
	Kind m_Kind;
	float m_Radius;
	float m_Life;
	float m_Age = 0.0f;
	float m_DamagePerSecond;
	float m_SlowFactor;
	float m_Tick = 0.0f;
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

	// 동행자의 스킬이 거는 것들
	void ApplySlow(float factor, float seconds);   // factor 0.4 = 40% 속도로
	void Push(float velocityX, float velocityY);   // 맞은 것과 별개로 더 세게 민다
	bool IsSlowed() const { return m_Data.slowTimer > 0.0f; }

	// 다음 한 대를 때린 마을 사람을 미리 일러 둔다. TakeDamage가 받아 적는다.
	// 일러 두지 않고 맞으면 플레이어(또는 동행자)가 때린 것으로 친다.
	void MarkHitBy(int villagerId) { m_PendingHitBy = villagerId; }

	// 첫 마을 외곽을 두드리는 무리로 만든다
	void MakeOutskirt() { m_Data.outskirt = true; }
	bool IsOutskirt() const { return m_Data.outskirt; }

	// Game이 적 목록을 들고 있으므로, 태어나고 사라질 때 알려 준다.
	void AttachToGame(Game* game);

private:
	void Die(const ActorContext& context);
	void DropLoot(const ActorContext& context);

	Enemy m_Data;
	Game* m_Game = NULL;
	int m_PendingHitBy = -1;
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
