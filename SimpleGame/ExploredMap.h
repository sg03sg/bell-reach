#pragma once

#include <unordered_map>
#include <vector>

//
// 플레이어가 불로 밝혀 본 적이 있는 타일의 기억.
//
// LightGrid의 발자국은 25초면 식어 사라지지만, 한 번 본 곳을 기억하는 건
// 사람이다. 지도는 이 기억만 보고 그린다 — 밝혀 본 적 없는 곳은 지도에도 없다.
//
// ── 흑백 그림 한 장 ──
// 기억은 구역(272x272칸)마다 흑백 그림 한 장으로 둔다. 칸 하나가 바이트 하나이고,
// 본 곳은 255(흰색), 못 본 곳은 0(검은색)이다. 지도를 그릴 때는 이 그림을 한 줄씩
// 훑기만 하면 되고, 마을 하나를 통째로 밝힐 때도 사각형을 흰색으로 칠하면 끝난다.
// 구역의 기억은 Mask()로 그대로 꺼내 볼 수 있다(파일로 떠 보기 쉽게 PGM과 같은 배치).
//
// 맵이 무한하므로 구역 단위로 필요할 때 만든다. 걸어간 만큼만 메모리를 쓴다.
//
class ExploredMap
{
public:
	static const unsigned char kSeen = 255;
	static const unsigned char kUnseen = 0;

	// 처음 본 칸이면 true. 지도 그림이 이 칸만 새로 칠한다.
	bool Mark(int tileX, int tileY);
	bool IsSeen(int tileX, int tileY) const;

	// 구역 하나의 흑백 그림. 한 변이 Span()칸, 0행이 구역의 맨 위. 본 적 없는 구역은 NULL.
	const unsigned char* Mask(int regionX, int regionY) const;
	static int Span();

	// 새로 밝힌 타일이 생길 때마다 오른다.
	unsigned int Version() const { return m_Version; }

private:
	std::vector<unsigned char>& SheetFor(int regionX, int regionY);

	std::unordered_map<long long, std::vector<unsigned char>> m_Sheets;
	unsigned int m_Version = 0;
};
