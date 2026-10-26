#pragma once

#include <unordered_map>
#include <vector>

//
// 무한 맵 — 역병이 휩쓸고 간 마을들이 숲 속에 한 줄로 이어진 섬.
//
// 타일 좌표에는 상한도 하한도 없다. 음수도 유효하다.
// 필요한 청크만 그때그때 생성해서 캐시하므로, 걸어간 곳만 메모리를 쓴다.
//
// 생성은 좌표만으로 결정된다(같은 씨앗 + 같은 좌표 = 항상 같은 지형).
// 저장할 것이 씨앗 하나뿐이고, 되돌아왔을 때 지형이 달라지지 않는다.
//
// ── 마을과 숲 ──
//
// 청크 17x17(272칸)이 구역 하나다. 구역은 둘 중 하나다.
//
//   마을 — Regions가 정한 길(첫 마을 → 둘째 마을 → …) 위의 구역
//   숲   — 그 밖의 모든 구역. 들어갈 수 없는 깊은 숲이다
//
// 마을은 울타리로 둘러싸인다. 첫 마을은 돌 성벽, 나머지는 나무 울타리다.
//
//      0          84                    188         271   (구역 안의 칸)
//      ┌──────────┬──────────────────────┬──────────┐
//      │   숲     │     울타리 안 103칸   │    숲    │
//      │          │  골목이 16칸마다     │          │
//      │ 길 ~~~~~~╪═══ 광장 · 종탑 ══════╪~~~~~~ 길 │  ← 135·136줄의 큰길
//      │ (굽은 숲길)│  가로세로로 엮인 성읍 │(굽은 숲길)│
//      └──────────┴──────────────────────┴──────────┘
//
// 문에서 구역 끝까지 숲길이 84칸쯤이라, 이웃 마을 문까지는 약 167칸(5,300px)이다.
// 1레벨 걸음(190px/s)으로 30초 남짓. 숲길은 kRoadBend칸 폭으로 굽는다 —
// 문 앞과 구역 경계에서는 곧게 이어져서 이웃 구역의 길과 늘 맞닿는다.
// 숲길은 문을 나서면 열두 칸(화면 높이의 절반 남짓)으로 넓어진다(kRoadSpread).
//
// 문은 길이 이어진 쪽에만 난다 — 앞 마을과 다음 마을 쪽. 그래서 길은
// 갈래가 없다. 첫 마을에서 나가면 둘째 마을에 닿고, 거기서 셋째로 간다.
//
// 울타리 안쪽을 따라 한 칸짜리 안길이 돌고, 큰길이 마을을 가로지른다.
// 울타리 밖 숲은 대부분 들어갈 수 없는 수풀(kWoods)이다. 나무 하나하나를
// 액터로 두면 너무 많으므로 땅이 대신 그리고, 길가처럼 사람이 다니는 곳에
// 닿은 숲 가장자리만 나무(kTree)로 세운다.
//
// 캐릭터가 갇히지 않도록 생성 마지막에 한 번 더 확인한다. 청크 안의 걸을 수
// 있는 칸을 길에서부터 채워 나가 닿지 않는 칸이 있으면, 그 칸을 막고 있는
// 나무를 베어 길을 연다. 집은 둘레 한 칸을 늘 비워 두므로 무언가를 가두지 않는다.
//
class Regions;

class World
{
public:
	static const int kChunkTiles = 16;

	// 타일 한 칸의 픽셀 크기. 충돌과 그리기가 모두 이 값을 쓴다.
	static const int kTilePixels = 32;

	// 마을 한 곳의 청크 수(한 변). Regions::kRegionChunks와 같아야 한다(Regions.cpp에서 검사).
	// 홀수여야 한가운데 청크에 광장과 종탑이 선다. 대부분은 울타리 밖 숲이다 —
	// 마을과 마을 사이를 1레벨 걸음으로 30초쯤 걸어야 닿게 하려는 것이다.
	static const int kVillageChunks = 17;

	// ── 마을 안의 자리(구역 안 칸 좌표, 0~47) ──
	static const int kVillageSpan = kVillageChunks * kChunkTiles;                         // 272
	static const int kVillageCenter = (kVillageChunks / 2) * kChunkTiles + kChunkTiles / 2; // 136. 종탑이 서는 칸
	static const int kVillageHalf = 52;                         // 종탑에서 울타리까지(칸)
	static const int kWallLow = kVillageCenter - kVillageHalf;  // 84. 울타리가 서는 줄
	static const int kWallHigh = kVillageCenter + kVillageHalf; // 188. 울타리 안은 103칸
	static const int kLaneLow = kWallLow + 1;    // 울타리 안쪽을 도는 안길
	static const int kLaneHigh = kWallHigh - 1;
	static const int kRoadLow = kVillageCenter - 1;   // 마을을 가로지르는 큰길(두 칸)
	static const int kRoadHigh = kVillageCenter;
	static const int kStreetLow = kVillageCenter - 2 * kChunkTiles;       // 104. 큰길에서 두 블록 떨어진 골목
	static const int kStreetHigh = kVillageCenter + 2 * kChunkTiles - 1;  // 167

	// 문 밖 숲길이 좌우로 굽는 폭(칸). 곧은 길 30초는 지루하다
	static const int kRoadBend = 5;

	// 숲길의 반폭(칸). 가운데 두 줄에서 양쪽으로 이만큼 — 모두 2 + 2 x 5 = 12칸(384px)이다.
	// 문 앞에서는 문 폭(네 칸)으로 시작해 점점 넓어진다.
	static const int kRoadSpread = 5;

	// ── 타일 ──
	static const unsigned char kGrass = 0;   // 풀밭
	static const unsigned char kPath  = 1;   // 흙길
	static const unsigned char kPlaza = 2;   // 종탑 광장의 돌바닥
	static const unsigned char kHouse = 3;   // 집이 선 자리. 지나갈 수 없다
	static const unsigned char kTree  = 4;   // 나무가 선 자리. 지나갈 수 없다
	static const unsigned char kWall  = 5;   // 울타리·성벽. 지나갈 수 없다
	static const unsigned char kWoods = 6;   // 깊은 숲. 지나갈 수 없고, 땅이 수풀로 그린다

	// ── 오브젝트 ──
	static const unsigned char kPropRubble = 0;   // 무너진 잔해
	static const unsigned char kPropSupply = 1;   // 보급품
	static const unsigned char kPropBarrel = 2;   // 나무통

	struct Prop
	{
		int tileX;
		int tileY;
		unsigned char kind;
	};

	// 집. (tileX, tileY)가 왼쪽 위 칸이고, 앞면(문이 난 쪽)은 아래를 향한다.
	struct House
	{
		int tileX;
		int tileY;
		int width;              // 가로 칸 수
		int depth;              // 세로 칸 수
		unsigned char style;    // 지붕 재료 등 겉모습
		bool ruined;            // 지붕이 내려앉은 폐가
		int door;               // -1 왼쪽, 0 가운데, 1 오른쪽
	};

	struct Tree
	{
		int tileX;
		int tileY;
		unsigned char kind;     // 0 = 활엽수, 1 = 침엽수, 2 = 말라 죽은 나무
	};

	// 울타리 한 칸
	struct Wall
	{
		int tileX;
		int tileY;
		unsigned char side;     // 0 북, 1 남, 2 서, 3 동 — 마을의 어느 쪽 울타리인가
		bool corner;            // 모서리. 굵은 기둥이 선다
		bool post;              // 문 옆 기둥. 높고, 되찾은 마을이면 등이 걸린다
		bool stone;             // 첫 마을의 돌 성벽인가
	};

	enum WallSide
	{
		kSideNorth = 0,
		kSideSouth = 1,
		kSideWest  = 2,
		kSideEast  = 3
	};

	explicit World(unsigned int seed = 20260908u);

	// 어느 구역이 마을이고 문이 어느 쪽으로 났는지는 Regions가 안다.
	// 지형을 만들기 전에 한 번 이어 준다. 이어 주지 않으면 모든 구역이
	// 사방으로 문이 난 마을이 된다(예전처럼).
	void SetRoute(const Regions* regions) { m_Route = regions; }

	unsigned char TileAt(int tileX, int tileY) const;
	bool IsBlocked(int tileX, int tileY) const;

	// 주어진 타일 범위에 걸치는 것들을 모은다. 필요한 범위만 요청할 것.
	void CollectProps(int tileMinX, int tileMinY,
	                  int tileMaxX, int tileMaxY,
	                  std::vector<Prop>& out) const;
	void CollectStructures(int tileMinX, int tileMinY,
	                       int tileMaxX, int tileMaxY,
	                       std::vector<House>& houses,
	                       std::vector<Tree>& trees) const;
	void CollectWalls(int tileMinX, int tileMinY,
	                  int tileMaxX, int tileMaxY,
	                  std::vector<Wall>& out) const;

	size_t CachedChunks() const { return m_Chunks.size(); }

	// 좌표를 청크 단위로 내림. 음수에서 0 방향으로 잘리지 않게 한다.
	static int FloorDiv(int value, int divisor);
	static int PositiveMod(int value, int divisor);

private:
	struct Chunk
	{
		std::vector<unsigned char> tiles;
		std::vector<Prop> props;
		std::vector<House> houses;
		std::vector<Tree> trees;
		std::vector<Wall> walls;
	};

	const Chunk& ChunkAt(int chunkX, int chunkY) const;
	void Generate(Chunk& chunk, int chunkX, int chunkY) const;

	unsigned int m_Seed;
	const Regions* m_Route = nullptr;

	// 조회하다 생성되므로 const 메서드에서도 쓸 수 있어야 한다.
	mutable std::unordered_map<long long, Chunk> m_Chunks;
};
