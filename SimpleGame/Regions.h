#pragma once

#include <unordered_map>
#include <vector>

//
// 구역 — 종탑 하나와 그 구역에 굳어 있는 사람 하나.
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
	// 구역 한 변의 청크 수. 3청크 = 48타일 = 1536픽셀이라
	// 화면(960)보다 넉넉히 커서 구역 하나가 한눈에 들어오지 않는다.
	static const int kRegionChunks = 3;

	struct Region
	{
		int regionX;
		int regionY;
		int towerTileX;
		int towerTileY;
		int sleeperTileX;
		int sleeperTileY;
		bool rung;      // 종을 울렸는가
		bool woken;     // 굳은 사람이 일어나 따라나섰는가

		// 0.0 = 완전히 굳음, 1.0 = 생기가 돌아옴.
		// 불을 비추면 차오르고, 등불이 떠나면 다시 식는다.
		float warmth;

		// 이 구역에 굳어 있는 사람이 누구인지 정하는 값. 좌표로 정해져 늘 같다.
		// 어떤 대사를 쓸지는 Game이 이 값으로 고른다.
		unsigned int personSeed;
	};

	explicit Regions(unsigned int seed = 20260908u);

	Region At(int regionX, int regionY) const;

	// 주어진 타일 범위에 걸치는 구역들을 모은다.
	void CollectOverlapping(int tileMinX, int tileMinY,
	                        int tileMaxX, int tileMaxY,
	                        std::vector<Region>& out) const;

	void MarkRung(int regionX, int regionY);
	void MarkWoken(int regionX, int regionY);
	void SetWarmth(int regionX, int regionY, float warmth);

	static int RegionOfTile(int tile);
	static int RegionTileSpan();

	int RungRegionCount() const { return m_RungCount; }

private:
	enum Flags
	{
		kFlagRung = 1,
		kFlagWoken = 2
	};

	struct State
	{
		unsigned char flags = 0;
		float warmth = 0.0f;
	};

	const State* FindState(int regionX, int regionY) const;

	unsigned int m_Seed;
	int m_RungCount = 0;
	std::unordered_map<long long, State> m_States;
};
