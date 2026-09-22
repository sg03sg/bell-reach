#include "stdafx.h"
#include "Profiler.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <vector>

namespace
{
	std::string TimeStamp()
	{
		std::time_t now = std::time(NULL);
		std::tm parts;
#ifdef _WIN32
		localtime_s(&parts, &now);
#else
		localtime_r(&now, &parts);
#endif
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d-%02d%02d%02d",
		              parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
		              parts.tm_hour, parts.tm_min, parts.tm_sec);
		return std::string(buffer);
	}
}

Profiler::Profiler(float reportInterval)
	: m_Interval(reportInterval)
	, m_RunId(TimeStamp())
{
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		m_Calls[i] = 0;
		m_Instances[i] = 0;
	}
}

Profiler::~Profiler()
{
}

void Profiler::SetLogPath(const std::string& path)
{
	m_LogPath = path;
	m_HeaderWritten = false;
}

void Profiler::AddFrame(const Frame& frame)
{
	m_Elapsed += frame.seconds;
	++m_Frames;
	m_WorstSeconds = std::max(m_WorstSeconds, frame.seconds);
	m_PostCalls += frame.postDrawCalls;

	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		m_Calls[i] += frame.sample.calls[i];
		m_Instances[i] += frame.sample.instances[i];
	}
	m_Flushes += frame.sample.flushes;
	m_KindBreaks += frame.sample.kindBreaks;

	const int total = frame.sample.TotalCalls() + frame.postDrawCalls;
	m_WorstDrawCalls = std::max(m_WorstDrawCalls, total);

	m_TotalSeconds += frame.seconds;
	++m_TotalFrames;
	m_TotalDrawCalls += total;
	m_TotalInstances += frame.sample.TotalInstances();
	m_SessionWorst = std::max(m_SessionWorst, frame.seconds);
}

bool Profiler::ShouldReport() const
{
	return m_Elapsed >= m_Interval && m_Frames > 0;
}

std::string Profiler::Report(const State& state)
{
	const float frames = static_cast<float>(std::max(1, m_Frames));
	const float averageFps = m_Frames / std::max(0.0001f, m_Elapsed);
	const float averageMs = m_Elapsed / frames * 1000.0f;
	const float worstMs = m_WorstSeconds * 1000.0f;

	// 가장 느렸던 한 프레임을 fps로 환산한 값. 체감 끊김은 이 숫자가 말해 준다.
	const float worstFps = (m_WorstSeconds > 0.0001f) ? (1.0f / m_WorstSeconds) : averageFps;

	long long callSum = 0;
	long long instanceSum = 0;
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		callSum += m_Calls[i];
		instanceSum += m_Instances[i];
	}

	const int post = static_cast<int>(m_PostCalls / frames + 0.5f);
	const int calls = static_cast<int>(callSum / frames + 0.5f) + post;
	const int instances = static_cast<int>(instanceSum / frames + 0.5f);
	const float ratio = (calls > 0) ? (static_cast<float>(instances) / calls) : 0.0f;

	std::string out;
	char line[512];

	std::snprintf(line, sizeof(line),
	              "[성능] fps %.1f (최저 %.1f)  |  프레임 %.2fms (최대 %.2fms)"
	              "  |  드로우 %d/프레임 (최대 %d)  |  도형 %d개  |  묶음 %.0f배  [%s]",
	              averageFps, worstFps, averageMs, worstMs,
	              calls, m_WorstDrawCalls, instances, ratio,
	              state.batching ? "모아서" : "하나씩");
	out += line;

	// ── 꼬리표별 내역. 도형 개수가 많은 순으로 위에서부터 ──
	{
		std::vector<std::pair<long long, int> > order;
		for (int i = 0; i < DrawSample::kTagCount; ++i)
		{
			if (m_Instances[i] > 0)
			{
				order.push_back(std::make_pair(m_Instances[i], i));
			}
		}
		std::sort(order.begin(), order.end(), std::greater<std::pair<long long, int> >());

		out += "\n[내역] ";
		const size_t show = std::min<size_t>(order.size(), 7);
		for (size_t i = 0; i < show; ++i)
		{
			const int tag = order[i].second;
			const int perFrame = static_cast<int>(order[i].first / frames + 0.5f);
			const float share = (instanceSum > 0)
			                  ? (100.0f * order[i].first / static_cast<float>(instanceSum)) : 0.0f;
			std::snprintf(line, sizeof(line), "%s%s %d(%.0f%%)",
			              (i > 0 ? " · " : ""), DrawTagName(static_cast<DrawTag>(tag)),
			              perFrame, share);
			out += line;
		}
		if (order.size() > show)
		{
			out += " · …";
		}
	}

	std::snprintf(line, sizeof(line),
	              "\n[묶음] 보낸 묶음 %.0f/프레임  ·  종류전환 %.0f  ·  후처리 %d  ·  CSV %s",
	              m_Flushes / frames, m_KindBreaks / frames, post,
	              m_LogPath.empty() ? "끔" : m_LogPath.c_str());
	out += line;

	if (!m_LogPath.empty())
	{
		WriteCsvRow(state, averageFps, worstFps, averageMs, worstMs);
	}

	// 창 비우기
	m_Elapsed = 0.0f;
	m_Frames = 0;
	m_WorstSeconds = 0.0f;
	m_PostCalls = 0;
	m_WorstDrawCalls = 0;
	m_Flushes = 0;
	m_KindBreaks = 0;
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		m_Calls[i] = 0;
		m_Instances[i] = 0;
	}

	return out;
}

void Profiler::WriteCsvHeader()
{
	std::ofstream file(m_LogPath.c_str(), std::ios::app);
	if (!file)
	{
		return;
	}

	file << "run,t_sec,fps_avg,fps_min,ms_avg,ms_max,draw_calls,draw_calls_max,"
	        "instances,flushes,kind_breaks,post_calls,batching,"
	        "level,health,enemies,actors,light_chunks,map_chunks,oil,rung,px,py";
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		file << ",inst_" << DrawTagKey(static_cast<DrawTag>(i));
	}
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		file << ",calls_" << DrawTagKey(static_cast<DrawTag>(i));
	}
	file << "\n";
	m_HeaderWritten = true;
}

void Profiler::WriteCsvRow(const State& state, float averageFps, float worstFps,
                           float averageMs, float worstMs)
{
	if (!m_HeaderWritten)
	{
		// 파일이 비어 있을 때만 머리글을 쓴다. 이어 붙이면 실행끼리 비교할 수 있다.
		std::ifstream probe(m_LogPath.c_str(), std::ios::ate | std::ios::binary);
		const bool empty = !probe || probe.tellg() == std::streampos(0);
		probe.close();
		if (empty)
		{
			WriteCsvHeader();
		}
		else
		{
			m_HeaderWritten = true;
		}
	}

	std::ofstream file(m_LogPath.c_str(), std::ios::app);
	if (!file)
	{
		return;
	}

	const float frames = static_cast<float>(std::max(1, m_Frames));
	long long callSum = 0;
	long long instanceSum = 0;
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		callSum += m_Calls[i];
		instanceSum += m_Instances[i];
	}

	file << m_RunId
	     << "," << m_TotalSeconds
	     << "," << averageFps
	     << "," << worstFps
	     << "," << averageMs
	     << "," << worstMs
	     << "," << (callSum / frames + m_PostCalls / frames)
	     << "," << m_WorstDrawCalls
	     << "," << (instanceSum / frames)
	     << "," << (m_Flushes / frames)
	     << "," << (m_KindBreaks / frames)
	     << "," << (m_PostCalls / frames)
	     << "," << (state.batching ? 1 : 0)
	     << "," << state.level
	     << "," << state.health
	     << "," << state.enemies
	     << "," << state.actors
	     << "," << state.lightChunks
	     << "," << state.mapChunks
	     << "," << state.oilPercent
	     << "," << state.rungRegions
	     << "," << state.playerX
	     << "," << state.playerY;

	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		file << "," << (m_Instances[i] / frames);
	}
	for (int i = 0; i < DrawSample::kTagCount; ++i)
	{
		file << "," << (m_Calls[i] / frames);
	}
	file << "\n";
}

std::string Profiler::SessionSummary() const
{
	if (m_TotalFrames == 0)
	{
		return std::string();
	}

	const double frames = static_cast<double>(m_TotalFrames);
	char line[512];
	std::snprintf(line, sizeof(line),
	              "[총평] %lld 프레임 · %.1f초 · 평균 %.1f fps (최악 %.1f fps)"
	              "  |  드로우 평균 %.0f/프레임  ·  도형 평균 %.0f/프레임",
	              m_TotalFrames, m_TotalSeconds,
	              frames / std::max(0.0001, m_TotalSeconds),
	              (m_SessionWorst > 0.0001f) ? (1.0f / m_SessionWorst) : 0.0f,
	              m_TotalDrawCalls / frames,
	              m_TotalInstances / frames);
	return std::string(line);
}
