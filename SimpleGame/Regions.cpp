#include "stdafx.h"
#include "Regions.h"

#include "World.h"

#include <algorithm>

// 구역 하나가 마을 하나다. 두 값이 어긋나면 종탑이 광장 밖에 서게 된다.
static_assert(Regions::kRegionChunks == World::kVillageChunks,
              "Regions::kRegionChunks must match World::kVillageChunks");

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

	// 굳은 사람이 설 수 있는 줄. 골목과 큰길이 엇갈리는 자리라 반드시 길 위이고,
	// 울타리 안이다. 3x3 아홉 자리 중 가운데는 종탑 자리라 뺀다.
	const int kSleeperGrid = 3;
	const int kSleeperLines[kSleeperGrid] = { World::kStreetLow, World::kVillageCenter, World::kStreetHigh };
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
	const int slots = kSleeperGrid * kSleeperGrid;
	const int center = slots / 2;

	int slot = static_cast<int>(hash % static_cast<unsigned int>(slots - 1));
	if (slot >= center)
	{
		++slot;   // 한가운데(종탑 자리)는 건너뛴다
	}

	const int span = RegionTileSpan();
	region.sleeperTileX = regionX * span + kSleeperLines[slot % kSleeperGrid];
	region.sleeperTileY = regionY * span + kSleeperLines[slot / kSleeperGrid];

	region.village = VillageIndex(regionX, regionY);
	region.gates = GatesOf(regionX, regionY);

	const State* state = FindState(regionX, regionY);
	region.rung   = state != NULL && (state->flags & kFlagRung) != 0;
	region.woken  = state != NULL && (state->flags & kFlagWoken) != 0;
	region.wardenDefeated = state != NULL && (state->flags & kFlagWardenDefeated) != 0;
	region.heard  = state != NULL && (state->flags & kFlagHeard) != 0;
	region.warmth = state != NULL ? state->warmth : 0.0f;
	region.personSeed = hash >> 8;   // 굳은 사람 자리를 고른 하위 비트와 겹치지 않게

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
			// 숲인 구역에는 종탑도 굳은 사람도 없다. 이 목록을 보는 쪽
			// (종탑 · 굳은 사람 · 종탑지기 · 미니맵 · 모니터)이 따로 거를 필요가 없게
			// 여기서 마을만 넘긴다.
			if (IsVillage(regionX, regionY))
			{
				out.push_back(At(regionX, regionY));
			}
		}
	}
}

// ═══ 마을이 이어지는 길 ═══════════════════════════════════════════
//
// 첫 마을(0,0)에서 한 걸음씩 늘려 간다. 걸음마다 동쪽으로 가거나, 남북으로 꺾는다.
//
//   · 서쪽으로는 가지 않는다 — 그래서 줄이 제 몸에 갇히지 않는다.
//     동쪽 칸은 늘 비어 있으므로 언제든 동쪽으로 나아갈 수 있다.
//   · 남북으로 꺾을 때는 같은 쪽으로만, 두 번까지만 이어 꺾는다.
//   · 꺾은 자리가 바로 서쪽 열의 마을과 맞닿으면 꺾지 않는다.
//     이어지지 않은 두 마을이 맞붙어 있으면 울타리 너머로 지름길처럼 보인다.
//
// 걸음마다 쓰는 난수는 (걸음 번호, 씨앗)으로 정해지므로 늘 같은 길이 나온다.
//
void Regions::ExtendRoute(int count) const
{
	if (m_Route.empty())
	{
		m_Route.push_back(std::make_pair(0, 0));
		m_RouteIndex[PackKey(0, 0)] = 0;
		m_VerticalRun = 0;
		m_LastStep = 0;
	}

	while (static_cast<int>(m_Route.size()) < count)
	{
		const int index = static_cast<int>(m_Route.size());
		const int x = m_Route.back().first;
		const int y = m_Route.back().second;
		const unsigned int roll = HashCoords(index, 7919, m_Seed);

		int turn = 0;   // -1 북, 1 남, 0이면 동쪽으로
		if (m_VerticalRun < 2 && (roll % 100u) < 45u)
		{
			int direction;
			if (m_LastStep == 1)      { direction = -1; }   // 북으로 꺾던 중이면 계속 북
			else if (m_LastStep == 2) { direction = 1; }    // 남으로 꺾던 중이면 계속 남
			else                      { direction = ((roll >> 8) & 1u) ? -1 : 1; }

			const int nextY = y + direction;
			const bool free = m_RouteIndex.find(PackKey(x, nextY)) == m_RouteIndex.end();
			const bool touchesWest = m_RouteIndex.find(PackKey(x - 1, nextY)) != m_RouteIndex.end();
			if (free && !touchesWest)
			{
				turn = direction;
			}
		}

		int nextX = x;
		int nextY = y;
		if (turn != 0)
		{
			nextY = y + turn;
			++m_VerticalRun;
			m_LastStep = (turn < 0) ? 1 : 2;
		}
		else
		{
			nextX = x + 1;
			m_VerticalRun = 0;
			m_LastStep = 0;
		}

		m_RouteIndex[PackKey(nextX, nextY)] = index;
		m_Route.push_back(std::make_pair(nextX, nextY));
	}
}

void Regions::ExtendRoutePast(int regionX) const
{
	// 동쪽으로만 나아가므로, 마지막 마을이 이 열을 넘어섰으면 이 열의 마을은 다 나왔다.
	ExtendRoute(1);
	while (m_Route.back().first <= regionX)
	{
		ExtendRoute(static_cast<int>(m_Route.size()) + 1);
	}
}

int Regions::VillageIndex(int regionX, int regionY) const
{
	if (regionX < 0)
	{
		return -1;   // 첫 마을 서쪽은 모두 숲이다
	}

	ExtendRoutePast(regionX);
	auto found = m_RouteIndex.find(PackKey(regionX, regionY));
	return found == m_RouteIndex.end() ? -1 : found->second;
}

void Regions::VillagePosition(int index, int& regionX, int& regionY) const
{
	ExtendRoute(std::max(1, index + 1));
	regionX = m_Route[static_cast<size_t>(index)].first;
	regionY = m_Route[static_cast<size_t>(index)].second;
}

unsigned char Regions::GatesOf(int regionX, int regionY) const
{
	const int index = VillageIndex(regionX, regionY);
	if (index < 0)
	{
		return 0;
	}

	ExtendRoute(index + 2);   // 다음 마을까지 알아야 그쪽 문을 낼 수 있다

	unsigned char gates = 0;
	auto toward = [&](int other)
	{
		const int dx = m_Route[static_cast<size_t>(other)].first - regionX;
		const int dy = m_Route[static_cast<size_t>(other)].second - regionY;
		if (dx > 0)      { gates |= kGateEast; }
		else if (dx < 0) { gates |= kGateWest; }
		else if (dy > 0) { gates |= kGateSouth; }
		else if (dy < 0) { gates |= kGateNorth; }
	};

	if (index > 0)
	{
		toward(index - 1);   // 앞 마을 쪽
	}
	toward(index + 1);       // 다음 마을 쪽
	return gates;
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

void Regions::MarkWardenDefeated(int regionX, int regionY)
{
	m_States[PackKey(regionX, regionY)].flags |= kFlagWardenDefeated;
}

void Regions::MarkHeard(int regionX, int regionY)
{
	m_States[PackKey(regionX, regionY)].flags |= kFlagHeard;
}

void Regions::CollectRung(std::vector<Region>& out) const
{
	for (const auto& entry : m_States)
	{
		if ((entry.second.flags & kFlagRung) == 0)
		{
			continue;
		}
		// 키에서 구역 좌표를 되돌린다 (PackKey의 역)
		const int regionY = static_cast<int>(entry.first >> 32);
		const int regionX = static_cast<int>(static_cast<unsigned int>(entry.first & 0xFFFFFFFFll));
		out.push_back(At(regionX, regionY));
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
