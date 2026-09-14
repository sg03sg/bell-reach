#include "stdafx.h"
#include "ExploredMap.h"

#include "World.h"

namespace
{
	long long PackKey(int chunkX, int chunkY)
	{
		return (static_cast<long long>(chunkY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(chunkX));
	}
}

void ExploredMap::Mark(int tileX, int tileY)
{
	const int n = World::kChunkTiles;
	std::vector<unsigned char>& cells =
		m_Chunks[PackKey(World::FloorDiv(tileX, n), World::FloorDiv(tileY, n))];

	if (cells.empty())
	{
		cells.assign(static_cast<size_t>(n) * n, 0);
	}

	unsigned char& cell = cells[World::PositiveMod(tileY, n) * n + World::PositiveMod(tileX, n)];
	if (cell == 0)
	{
		cell = 1;
		++m_Version;
	}
}

bool ExploredMap::IsSeen(int tileX, int tileY) const
{
	const int n = World::kChunkTiles;
	auto found = m_Chunks.find(PackKey(World::FloorDiv(tileX, n), World::FloorDiv(tileY, n)));
	if (found == m_Chunks.end())
	{
		return false;
	}
	return found->second[World::PositiveMod(tileY, n) * n + World::PositiveMod(tileX, n)] != 0;
}
