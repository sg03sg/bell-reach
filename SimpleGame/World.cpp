#include "stdafx.h"
#include "World.h"

#include <algorithm>

namespace
{
	// 좌표와 씨앗을 섞어 청크마다 고정된 난수 씨앗을 만든다.
	unsigned int HashCoords(int x, int y, unsigned int seed)
	{
		unsigned int h = seed;
		h ^= static_cast<unsigned int>(x) * 0x9E3779B9u;
		h = (h << 13) | (h >> 19);
		h *= 0x85EBCA6Bu;
		h ^= static_cast<unsigned int>(y) * 0xC2B2AE35u;
		h = (h << 17) | (h >> 15);
		h *= 0x27D4EB2Fu;
		h ^= h >> 16;
		return h ? h : 0x1234567u;   // xorshift는 0에서 벗어나지 못한다
	}

	long long PackKey(int chunkX, int chunkY)
	{
		return (static_cast<long long>(chunkY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(chunkX));
	}
}

World::World(unsigned int seed)
	: m_Seed(seed)
{
}

int World::FloorDiv(int value, int divisor)
{
	const int quotient = value / divisor;
	return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

int World::PositiveMod(int value, int divisor)
{
	const int remainder = value % divisor;
	return remainder < 0 ? remainder + divisor : remainder;
}

void World::Generate(Chunk& chunk, int chunkX, int chunkY) const
{
	chunk.tiles.assign(kChunkTiles * kChunkTiles, 1);
	chunk.props.clear();

	unsigned int rng = HashCoords(chunkX, chunkY, m_Seed);
	auto next = [&rng]()
	{
		rng ^= rng << 13;
		rng ^= rng >> 17;
		rng ^= rng << 5;
		return rng;
	};

	auto carve = [&chunk](int x0, int y0, int x1, int y1)
	{
		const int left   = std::max(0, std::min(x0, x1));
		const int right  = std::min(kChunkTiles - 1, std::max(x0, x1));
		const int top    = std::max(0, std::min(y0, y1));
		const int bottom = std::min(kChunkTiles - 1, std::max(y0, y1));

		for (int y = top; y <= bottom; ++y)
		{
			for (int x = left; x <= right; ++x)
			{
				chunk.tiles[y * kChunkTiles + x] = 0;
			}
		}
	};

	// 청크 한가운데를 관통하는 십자 통로.
	// 모든 청크가 네 변의 정중앙에서 열리므로, 이웃한 청크와 반드시 이어진다.
	// 무한 맵에서 길이 끊기지 않는다는 것을 생성 규칙만으로 보장하는 방법이다.
	const int mid = kChunkTiles / 2;
	carve(0, mid - 1, kChunkTiles - 1, mid);
	carve(mid - 1, 0, mid, kChunkTiles - 1);

	// 방 몇 개를 파고 십자 통로에 잇는다.
	const int roomCount = 2 + static_cast<int>(next() % 3);
	for (int i = 0; i < roomCount; ++i)
	{
		const int width  = 3 + static_cast<int>(next() % 5);
		const int height = 3 + static_cast<int>(next() % 5);
		const int left   = static_cast<int>(next() % (kChunkTiles - width));
		const int top    = static_cast<int>(next() % (kChunkTiles - height));

		carve(left, top, left + width - 1, top + height - 1);

		const int centerX = left + width / 2;
		const int centerY = top + height / 2;
		carve(centerX, centerY, mid, centerY);
		carve(centerX, centerY, centerX, mid);

		// 방마다 오브젝트를 하나씩. 어둠 속에서는 보이지 않는다.
		if (next() % 4 != 0)
		{
			Prop prop;
			prop.tileX = chunkX * kChunkTiles + left + static_cast<int>(next() % width);
			prop.tileY = chunkY * kChunkTiles + top + static_cast<int>(next() % height);
			prop.kind = static_cast<unsigned char>(next() % 4 == 0 ? 1 : 0);
			chunk.props.push_back(prop);
		}
	}
}

const World::Chunk& World::ChunkAt(int chunkX, int chunkY) const
{
	const long long key = PackKey(chunkX, chunkY);

	auto found = m_Chunks.find(key);
	if (found != m_Chunks.end())
	{
		return found->second;
	}

	Chunk chunk;
	Generate(chunk, chunkX, chunkY);
	return m_Chunks.emplace(key, std::move(chunk)).first->second;
}

bool World::IsWall(int tileX, int tileY) const
{
	const int chunkX = FloorDiv(tileX, kChunkTiles);
	const int chunkY = FloorDiv(tileY, kChunkTiles);
	const int localX = PositiveMod(tileX, kChunkTiles);
	const int localY = PositiveMod(tileY, kChunkTiles);

	return ChunkAt(chunkX, chunkY).tiles[localY * kChunkTiles + localX] != 0;
}

void World::CollectProps(int tileMinX, int tileMinY,
                         int tileMaxX, int tileMaxY,
                         std::vector<Prop>& out) const
{
	const int firstChunkX = FloorDiv(tileMinX, kChunkTiles);
	const int lastChunkX  = FloorDiv(tileMaxX, kChunkTiles);
	const int firstChunkY = FloorDiv(tileMinY, kChunkTiles);
	const int lastChunkY  = FloorDiv(tileMaxY, kChunkTiles);

	for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY)
	{
		for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX)
		{
			for (const Prop& prop : ChunkAt(chunkX, chunkY).props)
			{
				if (prop.tileX >= tileMinX && prop.tileX <= tileMaxX
				 && prop.tileY >= tileMinY && prop.tileY <= tileMaxY)
				{
					out.push_back(prop);
				}
			}
		}
	}
}
