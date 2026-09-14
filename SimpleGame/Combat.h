#pragma once

#include <string>

//
// 전투에 쓰는 것들의 모양과 수치.
//
// 적은 원래 이 섬의 주민이었다. 어둠에 완전히 삼켜진 자들이다.
// 어둠 속에서 나타나 등불을 향해 다가오지만, 불빛 안에서는 발이 무거워지고
// 종소리가 닿는 곳(되찾은 구역)에는 들어오지 못한다.
//
enum class EnemyKind
{
	Wraith,     // 떠도는 그림자 — 빠르고 약하다
	Husk,       // 삼켜진 자 — 느리고 질기다
	Warden      // 종탑지기 — 종탑을 지키는 우두머리
};

struct EnemyType
{
	const char* name;
	float health;           // 기본 체력. 섬 안쪽으로 갈수록 배율이 붙는다
	float speed;            // px/s
	float radius;           // 몸 크기. 부딪힘과 피격 판정에 쓴다
	float touchDamage;
	float touchCooldown;    // 붙어 있을 때 다시 때리기까지(초)
	int   souls;            // 떨구는 영혼석 개수
	int   soulValue;        // 영혼석 하나의 값
	int   healChance;       // 회복약을 떨굴 확률(%)
	int   weaponChance;     // 무기 강화석을 떨굴 확률(%)
	int   magnetChance;     // 자석을 떨굴 확률(%)
};

const EnemyType& EnemyTypeOf(EnemyKind kind);

struct Enemy
{
	int id = 0;
	EnemyKind kind = EnemyKind::Wraith;
	float x = 0.0f;
	float y = 0.0f;
	float health = 0.0f;
	float maxHealth = 0.0f;
	float power = 1.0f;       // 섬 안쪽으로 갈수록 커지는 배율
	float touchTimer = 0.0f;
	float hitFlash = 0.0f;    // 맞은 순간 하얗게 번쩍인다
	float knockX = 0.0f;
	float knockY = 0.0f;
	float phase = 0.0f;       // 몸짓 박자를 개체마다 다르게
	float dying = 0.0f;       // 0이면 살아 있다. 0보다 크면 흩어지는 중(초)

	// 종탑지기 전용
	int regionX = 0;
	int regionY = 0;
	float homeX = 0.0f;
	float homeY = 0.0f;
	bool engaged = false;     // 플레이어를 쫓는 중인가
	float burstTimer = 0.0f;  // 어둠 구슬을 흩뿌리기까지
};

// 등불에서 날아가는 불씨
struct Projectile
{
	float x = 0.0f;
	float y = 0.0f;
	float vx = 0.0f;
	float vy = 0.0f;
	float damage = 0.0f;
	float travel = 0.0f;      // 남은 비행 거리(px). 사거리를 넘으면 사그라든다
	int pierce = 0;           // 더 꿰뚫을 수 있는 적 수
	int targetId = -1;        // 쫓아가는 적. 없으면 곧게 난다
	int lastHitId = -1;       // 같은 적을 연달아 맞히지 않게
};

// 종탑지기가 흩뿌리는 어둠 구슬
struct Orb
{
	float x = 0.0f;
	float y = 0.0f;
	float vx = 0.0f;
	float vy = 0.0f;
	float damage = 0.0f;
	float life = 0.0f;        // 남은 시간(초)
};

enum class PickupKind
{
	Soul,       // 영혼석 — 경험치
	Heal,       // 회복약 — 체력
	Weapon,     // 강화석 — 무기 강화
	Magnet      // 자석 — 한동안 떨어진 것을 전부 끌어온다
};

struct Pickup
{
	PickupKind kind = PickupKind::Soul;
	float x = 0.0f;
	float y = 0.0f;
	float vx = 0.0f;
	float vy = 0.0f;
	int value = 1;
	float age = 0.0f;
};

// 머리 위로 떠올랐다 사라지는 글자. number가 0 이상이면 숫자를, 아니면 label을 쓴다.
struct FloatingText
{
	float x = 0.0f;
	float y = 0.0f;
	float age = 0.0f;
	float life = 0.9f;
	int number = -1;
	std::string label;
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float size = 16.0f;
};
