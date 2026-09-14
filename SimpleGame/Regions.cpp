#include "stdafx.h"
#include "Regions.h"

#include "World.h"

namespace
{
	unsigned int HashCoords(int x, int y, unsigned int seed)
	{
		unsigned int h = seed ^ 0x5BD1E995u;
		h ^= static_cast<unsigned int>(x) * 0x9E3779B9u;
		h = (h << 13) | (h >> 19);
		h *= 0x85EBCA6Bu;
		h ^= static_cast<unsigned int>(y) * 0xC2B2AE35u;
		h = (h << 17) | (h >> 15);
		h *= 0x27D4EB2Fu;
		h ^= h >> 16;
		return h;
	}

	long long PackKey(int x, int y)
	{
		return (static_cast<long long>(y) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(x));
	}

	// 청크 한가운데는 십자 통로가 교차하는 자리라 반드시 바닥이다.
	// 지형을 뒤져 빈 칸을 찾을 필요 없이 배치를 보장할 수 있다.
	int ChunkCenterTile(int chunkIndex)
	{
		return chunkIndex * World::kChunkTiles + World::kChunkTiles / 2;
	}
}

Regions::Regions(unsigned int seed)
	: m_Seed(seed)
{
}

int Regions::RegionTileSpan()
{
	return kRegionChunks * World::kChunkTiles;
}

int Regions::RegionOfTile(int tile)
{
	return World::FloorDiv(tile, RegionTileSpan());
}

const Regions::State* Regions::FindState(int regionX, int regionY) const
{
	auto found = m_States.find(PackKey(regionX, regionY));
	return found == m_States.end() ? NULL : &found->second;
}

Regions::Region Regions::At(int regionX, int regionY) const
{
	Region region;
	region.regionX = regionX;
	region.regionY = regionY;

	const int firstChunkX = regionX * kRegionChunks;
	const int firstChunkY = regionY * kRegionChunks;

	// 종탑은 구역 한가운데 청크에. 어느 방향에서 들어와도 대칭이다.
	region.towerTileX = ChunkCenterTile(firstChunkX + kRegionChunks / 2);
	region.towerTileY = ChunkCenterTile(firstChunkY + kRegionChunks / 2);

	// 굳은 사람은 종탑이 아닌 다른 청크에. 어느 청크인지는 좌표가 결정한다.
	const unsigned int hash = HashCoords(regionX, regionY, m_Seed);
	const int slots = kRegionChunks * kRegionChunks;
	const int center = slots / 2;

	int slot = static_cast<int>(hash % static_cast<unsigned int>(slots - 1));
	if (slot >= center)
	{
		++slot;   // 한가운데(종탑 자리)는 건너뛴다
	}

	region.sleeperTileX = ChunkCenterTile(firstChunkX + slot % kRegionChunks);
	region.sleeperTileY = ChunkCenterTile(firstChunkY + slot / kRegionChunks);

	const State* state = FindState(regionX, regionY);
	region.rung   = state != NULL && (state->flags & kFlagRung) != 0;
	region.woken  = state != NULL && (state->flags & kFlagWoken) != 0;
	region.warmth = state != NULL ? state->warmth : 0.0f;

	return region;
}

void Regions::CollectOverlapping(int tileMinX, int tileMinY,
                                 int tileMaxX, int tileMaxY,
                                 std::vector<Region>& out) const
{
	const int firstX = RegionOfTile(tileMinX);
	const int lastX  = RegionOfTile(tileMaxX);
	const int firstY = RegionOfTile(tileMinY);
	const int lastY  = RegionOfTile(tileMaxY);

	for (int regionY = firstY; regionY <= lastY; ++regionY)
	{
		for (int regionX = firstX; regionX <= lastX; ++regionX)
		{
			out.push_back(At(regionX, regionY));
		}
	}
}

void Regions::MarkRung(int regionX, int regionY)
{
	State& state = m_States[PackKey(regionX, regionY)];
	if ((state.flags & kFlagRung) == 0)
	{
		state.flags |= kFlagRung;
		++m_RungCount;
	}
}

void Regions::MarkWoken(int regionX, int regionY)
{
	State& state = m_States[PackKey(regionX, regionY)];
	state.flags |= kFlagWoken;
	state.warmth = 1.0f;
}

void Regions::SetWarmth(int regionX, int regionY, float warmth)
{
	// 아직 아무 일도 없었고 완전히 식은 상태라면 굳이 기록을 만들지 않는다.
	// 무한 맵을 걸어다니는 동안 빈 항목이 쌓이는 것을 막는다.
	if (warmth <= 0.0f && FindState(regionX, regionY) == NULL)
	{
		return;
	}

	m_States[PackKey(regionX, regionY)].warmth = warmth;
}
