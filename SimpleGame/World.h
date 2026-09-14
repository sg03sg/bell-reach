#pragma once

#include <unordered_map>
#include <vector>

//
// 무한 맵.
//
// 타일 좌표에는 상한도 하한도 없다. 음수도 유효하다.
// 필요한 청크만 그때그때 생성해서 캐시하므로, 걸어간 곳만 메모리를 쓴다.
//
// 생성은 좌표만으로 결정된다(같은 씨앗 + 같은 좌표 = 항상 같은 지형).
// 저장할 것이 씨앗 하나뿐이라 세이브가 가벼워지고, 되돌아왔을 때
// 지형이 달라지지 않는다.
//
// 이 절차적 생성기는 v0.2에서 "손으로 만든 구역"으로 대체하거나 섞을
// 자리다. 그때 Game은 World의 인터페이스만 보고 있으므로 바뀌지 않는다.
//
class World
{
public:
	static const int kChunkTiles = 16;

	struct Prop
	{
		int tileX;
		int tileY;
		unsigned char kind;   // 0 = 잔해, 1 = 보급품
	};

	explicit World(unsigned int seed = 20260908u);

	bool IsWall(int tileX, int tileY) const;

	// 주어진 타일 범위 안의 오브젝트를 모은다. 화면에 보이는 범위만 요청할 것.
	void CollectProps(int tileMinX, int tileMinY,
	                  int tileMaxX, int tileMaxY,
	                  std::vector<Prop>& out) const;

	size_t CachedChunks() const { return m_Chunks.size(); }

	// 좌표를 청크 단위로 내림. 음수에서 0 방향으로 잘리지 않게 한다.
	static int FloorDiv(int value, int divisor);
	static int PositiveMod(int value, int divisor);

private:
	struct Chunk
	{
		std::vector<unsigned char> tiles;   // 1 = 벽, 0 = 바닥
		std::vector<Prop> props;
	};

	const Chunk& ChunkAt(int chunkX, int chunkY) const;
	void Generate(Chunk& chunk, int chunkX, int chunkY) const;

	unsigned int m_Seed;

	// 조회하다 생성되므로 const 메서드에서도 쓸 수 있어야 한다.
	mutable std::unordered_map<long long, Chunk> m_Chunks;
};
