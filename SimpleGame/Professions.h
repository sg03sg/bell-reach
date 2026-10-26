#pragma once

//
// 첫 마을 사람들의 직업과 마을의 물가.
//
// 마을은 저절로 돌아간다. 사냥꾼은 문 밖 어둠을 잡아 현상금을 벌고,
// 사람들은 번 돈을 상인과 의사에게 쓰고, 돈이 모이면 제 집 둘레를 꾸민다.
// 가난한 좀도둑은 부자의 주머니를 노리고, 들키면 시비가 붙는다.
//
// 플레이어도 그 돈의 흐름에 들어간다. 어둠을 잡아 주운 골드로 상인에게서
// 횃불과 약과 기름을, 의사에게서 치료를, 정보상에게서 다음 마을 소식을,
// 사냥꾼에게서 강화석을 산다. 플레이어가 쓴 돈은 그 사람의 지갑으로 가고,
// 그 사람은 그 돈으로 마을을 꾸민다 — 내가 돈을 쓴 만큼 마을이 살아난다.
//
enum class Profession
{
	Commoner,    // 주민 — 일해서 조금씩 번다. 몇은 좀도둑이다
	Merchant,    // 상인 — 횃불, 회복약, 기름
	Hunter,      // 사냥꾼 — 문 밖에서 어둠을 잡는다. 강화석을 판다
	Doctor,      // 의사 — 다친 사람을 고친다
	Informant,   // 정보상 — 다음 마을 소식을 판다
	Guard,       // 경비 — 울타리 안쪽을 돌고 도둑을 잡는다
	Knight,      // 기사 — 이사 가는 사람과 편지를 지키며 숲길을 건넌다. 가장 든든하다
	Adventurer,  // 모험가 — 품삯을 받고 길을 함께 간다
	Courier,     // 전령 — 이웃 마을로 편지를 나른다
	Count
};

// 자기 구역을 꾸밀 때 놓는 것. 순서대로 하나씩 늘어난다.
enum class Decor : unsigned char
{
	Sign,        // 간판 — 가게라면 맨 먼저
	Lantern,     // 등 기둥 — 스스로 빛난다. 마을이 부유해질수록 밤이 밝아진다
	Planter,     // 화분
	Banner,      // 깃발 — 그 사람의 빛깔
	Bench,       // 벤치
	Crates,      // 쌓인 상자
	Count
};

struct ProfessionInfo
{
	const char* name;        // 화면에 나오는 이름
	float tint[3];           // 옷과 깃발의 빛깔
	int startGold;
	bool shop;               // 플레이어와 거래하는가
	Decor decor[6];          // 꾸미는 순서

	float maxHealth;         // 몸의 단단함
	float stoneDamage;       // 어둠에게 던지는 돌(기사는 칼로 친다고 치자)
	int escort;              // 숲길을 지키는 힘. 0이면 호위를 맡지 않는다
	int escortFee;           // 호위 한 번에 받는 돈
};

const ProfessionInfo& ProfessionOf(Profession profession);

//
// 모험가의 갈래.
//
// 모험가는 품삯을 받고 이사 가는 사람을 지켜 주기도 하지만, 플레이어도
// 돈을 주고 고용할 수 있다. 고용한 모험가는 다음 마을까지 함께 가며 제
// 갈래대로 싸운다(VillagerActor::ServeParty). 파티는 다섯 명까지다.
//
//   전사     붙어서 벤다. 칼끝 둘레의 적도 함께 맞는다
//   궁수     멀찍이 서서 쏜다. 적이 붙으면 물러나 거리를 벌린다
//   힐러     다친 사람(플레이어 · 파티)을 고친다. 한가하면 빛 조각을 던진다
//   마법사   무리 한가운데 불덩이를 떨군다. 느리지만 넓다
//   방패병   둘레의 적을 제게로 끈다. 맞아도 덜 아프다
//
enum class Calling : unsigned char
{
	None,
	Warrior,
	Archer,
	Healer,
	Mage,
	Guardian,
	Count
};

struct CallingInfo
{
	const char* name;      // 화면에 나오는 이름
	const char* role;      // 근접 · 원거리 · 회복 …
	const char* note;      // 고용 창의 한 줄
	float tint[3];         // 옷과 효과의 빛깔
	float maxHealth;
	float damage;          // 한 번에. 플레이어 레벨에 따라 커진다
	float reach;           // 이 안의 적을 친다(px)
	float every;           // 다시 치기까지(초)
	float toughness;       // 받는 피해 배율. 작을수록 단단하다
	int fee;               // 첫 마을에서의 품삯. 뒤쪽 마을일수록 비싸다
};

const CallingInfo& CallingOf(Calling calling);

const int kMaxParty = 5;   // 한 번에 데리고 다닐 수 있는 모험가

//
// 물가. 밸런스를 만질 때 이 한 곳만 보면 된다.
//
namespace Prices
{
	const int kTorch = 20;        // 상인 — 횃불 하나
	const int kHealPack = 15;     // 상인 — 회복약 하나
	const int kOil = 10;          // 상인 — 기름 절반
	const int kInfo = 25;         // 정보상 — 다음 마을 소식
	const int kWeaponStone = 60;  // 사냥꾼 — 강화석
	const int kDecor = 18;        // 마을 사람이 장식 하나를 놓는 데 드는 돈
	const int kVillagerShop = 6;  // 마을 사람이 상인에게서 장을 볼 때
	const int kVillagerDoctor = 6;// 마을 사람이 의사에게 치료받을 때

	// 플레이어 치료비. 잃은 체력 10%마다 2골드, 적어도 3골드.
	int DoctorFee(float health, float maxHealth);

	// 쓰러뜨린 적 하나에 마을 사람이 받는 현상금
	int Bounty(bool hunter, bool warden);

	const int kLetterReward = 15; // 플레이어가 편지를 이웃 마을에 전해 주면

	// 모험가를 다음 마을까지 고용하는 값. 뒤쪽 마을의 모험가일수록 숲길이 험해 비싸다.
	int HireFee(Calling calling, int village);
}

// 이웃 마을로 건너려면 일행에 이만큼의 호위 힘이 있어야 한다.
// 뒤쪽 마을로 갈수록 숲길의 어둠이 짙으므로 더 든든해야 떠난다.
int RequiredEscort(int fromVillage, int toVillage);

const int kMaxDecorations = 6;
const int kVillagersPerVillage = 20;   // 사람이 사는 마을 하나의 주민 수
const int kVillageCapacity = 26;       // 이사 와서 늘 수 있는 데까지
