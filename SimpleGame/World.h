#pragma once

#include <unordered_map>
#include <vector>

//
// 무한 맵 — 역병이 휩쓸고 간 마을들이 끝없이 이어진 섬.
//
// 타일 좌표에는 상한도 하한도 없다. 음수도 유효하다.
// 필요한 청크만 그때그때 생성해서 캐시하므로, 걸어간 곳만 메모리를 쓴다.
//
// 생성은 좌표만으로 결정된다(같은 씨앗 + 같은 좌표 = 항상 같은 지형).
// 저장할 것이 씨앗 하나뿐이고, 되돌아왔을 때 지형이 달라지지 않는다.
//
// 청크 3x3이 마을 하나다(= Regions의 구역 하나).
//
//   숲 │ 마을 │ 숲         가운데 : 종탑이 선 돌바닥 광장과 그 둘레의 집들
//   ───┼──────┼───        변     : 흙길을 따라 늘어선 집, 마당, 드문드문 나무
//  마을│ 광장 │마을       모서리 : 숲. 나무가 빽빽하고 버려진 집이 가끔 있다
//   ───┼──────┼───
//   숲 │ 마을 │ 숲
//
// 모든 청크는 한가운데를 가로지르는 십자 흙길을 가진다. 네 변 정중앙에서
// 이웃 청크의 길과 만나므로 길이 끊기지 않고, 종탑과 굳은 사람은 늘
// 그 교차점(청크 중심)에 놓여 걸어서 닿을 수 있다.
//
// 캐릭터가 갇히지 않도록 생성 마지막에 한 번 더 확인한다. 청크 안의 걸을 수
// 있는 칸을 길에서부터 채워 나가 닿지 않는 칸이 있으면, 그 칸을 막고 있는
// 나무를 베어 길을 연다. 집은 둘레 한 칸을 늘 비워 두므로 무언가를 가두지 않는다.
// 그래서 걸을 수 있는 모든 칸은 길과 이어져 있고, 적도 아이템도 닿지 못할
// 곳에 놓이지 않는다.
//
class World
{
public:
	static const int kChunkTiles = 16;

	// 타일 한 칸의 픽셀 크기. 충돌과 그리기가 모두 이 값을 쓴다.
	static const int kTilePixels = 32;

	// 마을 한 곳의 청크 수. Regions::kRegionChunks와 같아야 한다(Regions.cpp에서 검사).
	static const int kVillageChunks = 3;

	// ── 타일 ──
	static const unsigned char kGrass = 0;   // 풀밭
	static const unsigned char kPath  = 1;   // 흙길
	static const unsigned char kPlaza = 2;   // 종탑 광장의 돌바닥
	static const unsigned char kHouse = 3;   // 집이 선 자리. 지나갈 수 없다
	static const unsigned char kTree  = 4;   // 나무가 선 자리. 지나갈 수 없다

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

	explicit World(unsigned int seed = 20260908u);

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
	};

	const Chunk& ChunkAt(int chunkX, int chunkY) const;
	void Generate(Chunk& chunk, int chunkX, int chunkY) const;

	unsigned int m_Seed;

	// 조회하다 생성되므로 const 메서드에서도 쓸 수 있어야 한다.
	mutable std::unordered_map<long long, Chunk> m_Chunks;
};
