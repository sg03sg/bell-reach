#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

//
// 구역 — 종탑 하나와 그 구역에 굳어 있는 사람 하나.
//
// 모든 구역이 마을인 것은 아니다. 마을은 첫 마을(0,0)에서 시작해 한 줄로
// 이어지고, 그 밖의 구역은 들어갈 수 없는 숲이다. 이 줄이 곧 여정이다.
//
//   첫 마을 ─ 둘째 ─ 셋째
//                     │
//                    넷째 ─ 다섯째 ─ …
//
// 줄은 씨앗으로 정해지고, 대체로 동쪽으로 나아가며 가끔 남북으로 꺾인다.
// 서쪽으로는 돌아가지 않는다. 그래서 줄이 제 몸에 갇히는 일이 없고,
// 이어지지 않은 두 마을이 서로 맞닿지도 않는다 — 지름길이 생기지 않는다.
//
// 마을의 순서가 곧 난이도다. 적의 세기는 원점에서 얼마나 먼가가 아니라
// 몇 번째 마을인가로 정한다(Game::PowerAt).
//
// 배치는 구역 좌표만으로 결정된다. 저장할 것이 없고, 되돌아와도 그대로다.
// 바뀌는 것(종을 울렸는가 / 사람을 깨웠는가 / 얼마나 생기가 돌아왔는가)만
// 따로 기록한다. 지형은 재생성하고 변경된 것만 저장한다 —
// 무한 맵에서 표준적인 방식이다.
//
// 좌표는 전부 타일 단위다. 픽셀 변환은 Game이 그리기 직전에만 한다.
//
class Regions
{
public:
	// 구역 한 변의 청크 수. 5청크 = 80타일 = 2560픽셀.
	// 울타리 안 마을만 해도 화면(960)의 거의 두 배라 걸어 다니며 둘러봐야 한다.
	static const int kRegionChunks = 5;

	// 문이 난 쪽. 앞 마을과 다음 마을로 이어지는 쪽에만 난다.
	static const unsigned char kGateNorth = 1;
	static const unsigned char kGateEast  = 2;
	static const unsigned char kGateSouth = 4;
	static const unsigned char kGateWest  = 8;

	struct Region
	{
		int regionX;
		int regionY;

		int village;            // 몇 번째 마을인가. 0 = 첫 마을, -1 = 마을이 아닌 숲
		unsigned char gates;    // 문이 난 쪽(kGate* 비트)
		int towerTileX;
		int towerTileY;
		int sleeperTileX;
		int sleeperTileY;
		bool rung;      // 종을 울렸는가
		bool woken;     // 굳은 사람이 일어나 따라나섰는가
		bool wardenDefeated;   // 종탑지기를 쓰러뜨렸는가. 쓰러뜨려야 종을 울릴 수 있다

		// 0.0 = 완전히 굳음, 1.0 = 생기가 돌아옴.
		// 불을 비추면 차오르고, 등불이 떠나면 다시 식는다.
		float warmth;

		// 이 구역에 굳어 있는 사람이 누구인지 정하는 값. 좌표로 정해져 늘 같다.
		// 어떤 대사를 쓸지는 Game이 이 값으로 고른다.
		unsigned int personSeed;
	};

	explicit Regions(unsigned int seed = 20260908u);

	Region At(int regionX, int regionY) const;

	// ── 마을이 이어지는 길 ──
	// 몇 번째 마을인가. 마을이 아니면 -1.
	int VillageIndex(int regionX, int regionY) const;
	bool IsVillage(int regionX, int regionY) const { return VillageIndex(regionX, regionY) >= 0; }

	// index번째 마을의 구역 좌표
	void VillagePosition(int index, int& regionX, int& regionY) const;

	// 이 구역의 문이 난 쪽. 마을이 아니면 0.
	unsigned char GatesOf(int regionX, int regionY) const;

	// 주어진 타일 범위에 걸치는 마을들을 모은다. 숲인 구역은 넣지 않는다.
	void CollectOverlapping(int tileMinX, int tileMinY,
	                        int tileMaxX, int tileMaxY,
	                        std::vector<Region>& out) const;

	void MarkRung(int regionX, int regionY);
	void MarkWardenDefeated(int regionX, int regionY);

	// 종을 울린 구역을 모두 모은다. 쓰러졌을 때 돌아갈 곳을 찾는 데 쓴다.
	void CollectRung(std::vector<Region>& out) const;
	void MarkWoken(int regionX, int regionY);
	void SetWarmth(int regionX, int regionY, float warmth);

	static int RegionOfTile(int tile);
	static int RegionTileSpan();

	int RungRegionCount() const { return m_RungCount; }

private:
	enum Flags
	{
		kFlagRung = 1,
		kFlagWoken = 2,
		kFlagWardenDefeated = 4
	};

	struct State
	{
		unsigned char flags = 0;
		float warmth = 0.0f;
	};

	const State* FindState(int regionX, int regionY) const;

	// 길을 필요한 만큼만 늘린다. 조회하다 늘어나므로 const에서도 부를 수 있다.
	void ExtendRoute(int count) const;
	void ExtendRoutePast(int regionX) const;

	unsigned int m_Seed;

	// index번째 마을의 자리, 그리고 그 반대(자리 → index)
	mutable std::vector<std::pair<int, int> > m_Route;
	mutable std::unordered_map<long long, int> m_RouteIndex;
	mutable int m_VerticalRun = 0;    // 남북으로 몇 번 연달아 꺾였나
	mutable int m_LastStep = 0;       // 0 동, 1 북, 2 남
	int m_RungCount = 0;
	std::unordered_map<long long, State> m_States;
};
