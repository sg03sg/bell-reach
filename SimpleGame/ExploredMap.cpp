#include "stdafx.h"
#include "ExploredMap.h"

#include "Regions.h"
#include "World.h"

namespace
{
	long long PackKey(int regionX, int regionY)
	{
		return (static_cast<long long>(regionY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(regionX));
	}
}

int ExploredMap::Span()
{
	return Regions::RegionTileSpan();
}

std::vector<unsigned char>& ExploredMap::SheetFor(int regionX, int regionY)
{
	std::vector<unsigned char>& sheet = m_Sheets[PackKey(regionX, regionY)];
	if (sheet.empty())
	{
		// 클래스 상수를 참조로 넘기면 링크 에러가 날 수 있어 복사해 넘긴다.
		const unsigned char unseen = kUnseen;
		sheet.assign(static_cast<size_t>(Span()) * Span(), unseen);
	}
	return sheet;
}

bool ExploredMap::Mark(int tileX, int tileY)
{
	const int span = Span();
	std::vector<unsigned char>& sheet = SheetFor(Regions::RegionOfTile(tileX), Regions::RegionOfTile(tileY));

	unsigned char& cell = sheet[static_cast<size_t>(World::PositiveMod(tileY, span)) * span
	                            + World::PositiveMod(tileX, span)];
	if (cell == kSeen)
	{
		return false;
	}
	cell = kSeen;
	++m_Version;
	return true;
}

bool ExploredMap::IsSeen(int tileX, int tileY) const
{
	const int span = Span();
	const unsigned char* mask = Mask(Regions::RegionOfTile(tileX), Regions::RegionOfTile(tileY));
	if (mask == NULL)
	{
		return false;
	}
	return mask[static_cast<size_t>(World::PositiveMod(tileY, span)) * span
	            + World::PositiveMod(tileX, span)] == kSeen;
}

const unsigned char* ExploredMap::Mask(int regionX, int regionY) const
{
	auto found = m_Sheets.find(PackKey(regionX, regionY));
	return (found == m_Sheets.end()) ? NULL : found->second.data();
}
