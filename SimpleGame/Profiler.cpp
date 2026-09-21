#include "stdafx.h"
#include "Profiler.h"

#include <algorithm>
#include <cstdio>

Profiler::Profiler(float reportInterval)
	: m_Interval(reportInterval)
{
}

void Profiler::AddFrame(const Frame& frame)
{
	m_Elapsed += frame.seconds;
	++m_Frames;
	m_WorstSeconds = std::max(m_WorstSeconds, frame.seconds);

	m_SceneCalls += frame.sceneDrawCalls;
	m_PostCalls += frame.postDrawCalls;
	m_HudCalls += frame.hudDrawCalls;

	const int total = frame.sceneDrawCalls + frame.postDrawCalls + frame.hudDrawCalls;
	m_WorstDrawCalls = std::max(m_WorstDrawCalls, total);
}

bool Profiler::ShouldReport() const
{
	return m_Elapsed >= m_Interval && m_Frames > 0;
}

std::string Profiler::Report()
{
	const float frames = static_cast<float>(std::max(1, m_Frames));
	const float averageFps = m_Frames / std::max(0.0001f, m_Elapsed);
	const float averageMs = m_Elapsed / frames * 1000.0f;
	const float worstMs = m_WorstSeconds * 1000.0f;

	// 가장 느렸던 한 프레임을 fps로 환산한 값. 체감 끊김은 이 숫자가 말해 준다.
	const float worstFps = (m_WorstSeconds > 0.0001f) ? (1.0f / m_WorstSeconds) : averageFps;

	const int scene = static_cast<int>(m_SceneCalls / frames + 0.5f);
	const int post = static_cast<int>(m_PostCalls / frames + 0.5f);
	const int hud = static_cast<int>(m_HudCalls / frames + 0.5f);

	char line[256];
	std::snprintf(line, sizeof(line),
	              "[성능] fps %.1f (최저 %.1f)  |  프레임 %.2fms (최대 %.2fms)"
	              "  |  드로우 %d/프레임 (최대 %d) = 장면 %d · 후처리 %d · UI %d",
	              averageFps, worstFps, averageMs, worstMs,
	              scene + post + hud, m_WorstDrawCalls, scene, post, hud);

	m_Elapsed = 0.0f;
	m_Frames = 0;
	m_WorstSeconds = 0.0f;
	m_SceneCalls = 0;
	m_PostCalls = 0;
	m_HudCalls = 0;
	m_WorstDrawCalls = 0;

	return std::string(line);
}
