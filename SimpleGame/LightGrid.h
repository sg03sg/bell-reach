#pragma once

#include <vector>

//
// 이 게임의 핵심 자료구조.
//
// 조명, 안전 구역, 몬스터 이동 제한, 동료 침식이 전부 이 격자 하나를 조회한다.
// 밝기는 두 층으로 나뉜다.
//
//   Live  - 이번 프레임에 광원이 실제로 비추고 있는 세기. 매 프레임 다시 계산된다.
//   Trail - 한 번 밝혀진 칸이 기억하는 잔여 수명. 시간이 지나면 저절로 꺼진다.
//
// 최종 밝기는 두 값 중 큰 쪽이다. 플레이어가 지나간 자리가 잠시 밝게 남았다가
// 서서히 꺼지는 "등불 발자국"이 Trail 층에서 나온다.
//
class LightGrid
{
public:
	void Initialize(int cols, int rows, float cellSize);

	// 프레임 순서: BeginFrame -> AddLight(광원마다) -> Decay
	void BeginFrame();
	void AddLight(float worldX, float worldY, float radius, bool leavesTrail);
	void Decay(float deltaSeconds);

	float Brightness(int cellX, int cellY) const;   // 0.0 = 완전한 어둠, 1.0 = 밝음
	float BrightnessAt(float worldX, float worldY) const;
	float Glow(int cellX, int cellY) const;         // 현재 광원 성분만. 따뜻한 색조용

	int Cols() const { return m_Cols; }
	int Rows() const { return m_Rows; }
	float CellSize() const { return m_CellSize; }

private:
	int Index(int cellX, int cellY) const { return cellY * m_Cols + cellX; }
	bool InBounds(int cellX, int cellY) const;

	int m_Cols = 0;
	int m_Rows = 0;
	float m_CellSize = 1.0f;

	// 발자국이 완전히 꺼지기까지의 시간. 기획서 기준 20~30초.
	float m_TrailLife = 25.0f;

	// 잔여 수명이 이 비율 아래로 떨어져야 어두워지기 시작한다.
	// 0.2 * 25초 = 마지막 5초 동안 급격히 꺼진다.
	float m_FadeTail = 0.2f;

	// 발자국이 도달할 수 있는 최대 밝기.
	// 등불과 같은 밝기로 두면 화면이 한 덩어리가 되어 등불의 의미가 사라진다.
	// 잔광은 광원이 아니라 기억이므로 확실히 어두워야 한다.
	float m_TrailCeiling = 0.42f;

	std::vector<float> m_Trail;  // 잔여 수명 비율 0..1
	std::vector<float> m_Live;   // 이번 프레임 광원 세기 0..1
};
