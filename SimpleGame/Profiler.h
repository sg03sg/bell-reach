#pragma once

#include <string>

//
// 성능 계측.
//
// 프레임마다 걸린 시간과 그린 횟수를 모아 두었다가 일정 간격으로 한 줄 요약한다.
//
// 평균만 보면 뚝뚝 끊기는 순간을 놓친다. 60fps로 돌다가 한 프레임만 100ms를
// 먹어도 평균은 멀쩡해 보이기 때문에, 창 안에서 가장 나빴던 값을 함께 남긴다.
//
// 드로우 콜은 glDrawArrays를 부른 횟수다. 어디서 몇 번 불렀는지 나눠 세면
// 느려졌을 때 범인을 바로 찾을 수 있다 — 액터가 많아진 것인지, 후처리가
// 무거운 것인지, UI가 글자를 너무 그리는 것인지.
//
class Profiler
{
public:
	struct Frame
	{
		float seconds = 0.0f;        // 이 프레임에 걸린 시간
		int sceneDrawCalls = 0;      // 장면 — 땅과 액터
		int postDrawCalls = 0;       // 후처리 — Bloom, 가장자리 흐림, 합성
		int hudDrawCalls = 0;        // 화면에 붙는 것 — 체력, 대화창, 미니맵
	};

	explicit Profiler(float reportInterval = 1.0f);

	void AddFrame(const Frame& frame);

	// 요약을 낼 때가 되었는가
	bool ShouldReport() const;

	// 한 줄로 요약하고, 모아 둔 것을 비운다
	std::string Report();

private:
	float m_Interval;
	float m_Elapsed = 0.0f;
	int m_Frames = 0;

	float m_WorstSeconds = 0.0f;
	long long m_SceneCalls = 0;
	long long m_PostCalls = 0;
	long long m_HudCalls = 0;
	int m_WorstDrawCalls = 0;
};
