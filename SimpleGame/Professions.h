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
};

const ProfessionInfo& ProfessionOf(Profession profession);

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
}

const int kMaxDecorations = 6;
