#pragma once

#include <unordered_map>
#include <vector>

//
// 이 게임의 핵심 자료구조.
//
// 조명, 안개를 걷어내는 범위, 오브젝트 가시성, 앞으로 붙을 몬스터 이동 제한과
// 동료 침식까지 전부 이 격자 하나를 조회한다.
//
// 밝기는 두 층으로 나뉜다.
//
//   Live  - 이번 프레임에 광원이 실제로 비추고 있는 세기. 매 프레임 다시 계산된다.
//   Trail - 한 번 밝혀진 칸이 기억하는 잔여 수명. 시간이 지나면 저절로 꺼진다.
//
// 최종 밝기는 두 값 중 큰 쪽이다. 등불 발자국이 Trail 층에서 나온다.
//
// 맵이 무한하므로 격자도 청크 단위로 필요할 때 만들어진다.
// 발자국 수명이 유한하기 때문에 살아있는 청크 수는 저절로 한계를 갖는다.
// 완전히 식은 청크는 버려서 오래 걸어다녀도 메모리가 늘지 않는다.
//
class LightGrid
{
public:
	static const int kChunkCells = 32;

	void Configure(float cellSize);

	// 프레임 순서: BeginFrame -> AddLight(광원마다) -> Decay
	void BeginFrame();
	void AddLight(float worldX, float worldY, float radius, bool leavesTrail);
	void Decay(float deltaSeconds);

	float Brightness(int cellX, int cellY) const;   // 0.0 = 완전한 어둠, 1.0 = 밝음
	float BrightnessAt(float worldX, float worldY) const;
	float Glow(int cellX, int cellY) const;         // 현재 광원 성분만. 따뜻한 색조용

	int CellOf(float world) const;
	float CellSize() const { return m_CellSize; }
	size_t LiveChunks() const { return m_Chunks.size(); }

private:
	struct Chunk
	{
		std::vector<float> trail;   // 잔여 수명 비율 0..1
		std::vector<float> live;    // 이번 프레임 광원 세기 0..1
	};

	const Chunk* Find(int chunkX, int chunkY) const;
	Chunk& Touch(int chunkX, int chunkY);

	float m_CellSize = 16.0f;

	// 발자국이 완전히 꺼지기까지의 시간. 기획서 기준 20~30초.
	float m_TrailLife = 25.0f;

	// 잔여 수명이 이 비율 아래로 떨어져야 어두워지기 시작한다.
	// 0.2 * 25초 = 마지막 5초 동안 급격히 꺼진다.
	float m_FadeTail = 0.2f;

	// 발자국이 도달할 수 있는 최대 밝기.
	// 등불과 같은 밝기로 두면 화면이 한 덩어리가 되어 등불의 의미가 사라진다.
	// 잔광은 광원이 아니라 기억이므로 확실히 어두워야 한다.
	float m_TrailCeiling = 0.42f;

	std::unordered_map<long long, Chunk> m_Chunks;
};
