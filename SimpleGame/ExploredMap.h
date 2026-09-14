#pragma once

#include <unordered_map>
#include <vector>

//
// 플레이어가 불로 밝혀 본 적이 있는 타일의 기억.
//
// LightGrid의 발자국은 25초면 식어 사라지지만, 한 번 본 곳을 기억하는 건
// 사람이다. 미니맵은 이 기억만 보고 그린다 — 밝혀 본 적 없는 곳은 지도에도 없다.
//
// 맵이 무한하므로 청크 단위로 필요할 때 만든다. 걸어간 만큼만 메모리를 쓴다.
//
class ExploredMap
{
public:
	void Mark(int tileX, int tileY);
	bool IsSeen(int tileX, int tileY) const;

	// 새로 밝힌 타일이 생길 때마다 오른다. 미니맵을 다시 그릴지 판단하는 데 쓴다.
	unsigned int Version() const { return m_Version; }

private:
	std::unordered_map<long long, std::vector<unsigned char>> m_Chunks;
	unsigned int m_Version = 0;
};
