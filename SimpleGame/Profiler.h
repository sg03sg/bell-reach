#pragma once

#include <string>

#include "DrawStats.h"

//
// 성능 계측.
//
// 프레임마다 걸린 시간과 그린 내역을 모아 두었다가 일정 간격으로 요약한다.
// 요약은 두 군데로 나간다.
//
//   콘솔  - 지금 무슨 일이 일어나는지 사람이 읽는 용도. 서너 줄
//   CSV   - 나중에 분석하는 용도. 매 요약마다 한 줄씩 쌓인다
//
// 평균만 보면 뚝뚝 끊기는 순간을 놓친다. 60fps로 돌다가 한 프레임만 100ms를
// 먹어도 평균은 멀쩡해 보이기 때문에, 창 안에서 가장 나빴던 값을 함께 남긴다.
//
// 드로우 콜과 도형 개수를 따로 세는 것이 핵심이다.
//
//   드로우 콜 - 드라이버에 보낸 명령 수. 모아 그리면 확 줄어든다
//   도형 개수 - 실제로 그려진 것의 수. 모아 그려도 줄지 않는다
//
// 둘의 비가 곧 모아 그리기가 얼마나 듣고 있는지다. 그리고 도형 개수는
// 모아 그리기로 해결되지 않는 부분 — 애초에 너무 많이 그리고 있는지 —
// 를 보여 주므로, 다음에 무엇을 고쳐야 할지 여기서 읽는다.
//
class Profiler
{
public:
	struct Frame
	{
		float seconds = 0.0f;        // 이 프레임에 걸린 시간(자르지 않은 실측값)
		int sceneDrawCalls = 0;      // 장면 — 땅과 액터
		int postDrawCalls = 0;       // 후처리 — Bloom, 가장자리 흐림, 합성
		int hudDrawCalls = 0;        // 화면에 붙는 것 — 체력, 대화창, 미니맵
		DrawSample sample;           // 꼬리표별 내역
	};

	// 로그에 함께 남길 게임 상태. 성능 숫자만 남기면 나중에 봤을 때
	// "왜 이때 느렸지"를 알 수 없다.
	struct State
	{
		int level = 0;
		int health = 0;
		int enemies = 0;
		int actors = 0;
		int lightChunks = 0;
		int mapChunks = 0;
		int oilPercent = 0;
		int rungRegions = 0;
		float playerX = 0.0f;
		float playerY = 0.0f;
		bool batching = true;
		const char* companion = "";
	};

	explicit Profiler(float reportInterval = 1.0f);
	~Profiler();

	void AddFrame(const Frame& frame);

	// 요약을 낼 때가 되었는가
	bool ShouldReport() const;

	// 콘솔에 낼 여러 줄을 돌려주고, CSV에 한 줄 적고, 모아 둔 것을 비운다.
	std::string Report(const State& state);

	// CSV를 남길 폴더. 실행마다 run-<시각>.csv 를 새로 만든다.
	// 비우면 남기지 않는다.
	void SetLogPath(const std::string& folder);

	// 실행이 끝날 때 한 번. 처음부터 끝까지의 요약을 콘솔에 낸다.
	std::string SessionSummary() const;

private:
	void WriteCsvHeader();
	void WriteCsvRow(const State& state, float averageFps, float worstFps,
	                 float averageMs, float worstMs);

	float m_Interval;
	float m_Elapsed = 0.0f;
	int m_Frames = 0;

	float m_WorstSeconds = 0.0f;
	long long m_PostCalls = 0;
	int m_WorstDrawCalls = 0;

	// 꼬리표별 누적
	long long m_Calls[DrawSample::kTagCount];
	long long m_Instances[DrawSample::kTagCount];
	long long m_Flushes = 0;
	long long m_KindBreaks = 0;

	// 처음부터 끝까지
	double m_TotalSeconds = 0.0;
	long long m_TotalFrames = 0;
	long long m_TotalDrawCalls = 0;
	long long m_TotalInstances = 0;
	float m_SessionWorst = 0.0f;

	std::string m_LogPath;
	std::string m_RunId;
	bool m_HeaderWritten = false;
};
