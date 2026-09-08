#include "stdafx.h"
#include "LightGrid.h"

#include <algorithm>
#include <cmath>

void LightGrid::Initialize(int cols, int rows, float cellSize)
{
	m_Cols = cols;
	m_Rows = rows;
	m_CellSize = cellSize;

	m_Trail.assign(static_cast<size_t>(cols) * rows, 0.0f);
	m_Live.assign(static_cast<size_t>(cols) * rows, 0.0f);
}

bool LightGrid::InBounds(int cellX, int cellY) const
{
	return cellX >= 0 && cellX < m_Cols && cellY >= 0 && cellY < m_Rows;
}

void LightGrid::BeginFrame()
{
	std::fill(m_Live.begin(), m_Live.end(), 0.0f);
}

void LightGrid::AddLight(float worldX, float worldY, float radius, bool leavesTrail)
{
	if (radius <= 0.0f || m_Cols == 0)
	{
		return;
	}

	// 광원이 닿는 칸만 훑는다. 전체 격자를 도는 것보다 훨씬 싸다.
	const int minX = std::max(0, static_cast<int>((worldX - radius) / m_CellSize));
	const int maxX = std::min(m_Cols - 1, static_cast<int>((worldX + radius) / m_CellSize));
	const int minY = std::max(0, static_cast<int>((worldY - radius) / m_CellSize));
	const int maxY = std::min(m_Rows - 1, static_cast<int>((worldY + radius) / m_CellSize));

	// 안쪽은 균일하게 밝고 바깥으로 갈수록 부드럽게 떨어진다.
	const float inner = radius * 0.55f;

	for (int cy = minY; cy <= maxY; ++cy)
	{
		for (int cx = minX; cx <= maxX; ++cx)
		{
			const float centerX = (cx + 0.5f) * m_CellSize;
			const float centerY = (cy + 0.5f) * m_CellSize;
			const float dx = centerX - worldX;
			const float dy = centerY - worldY;
			const float distance = std::sqrt(dx * dx + dy * dy);

			if (distance >= radius)
			{
				continue;
			}

			float glow = 1.0f;
			if (distance > inner)
			{
				glow = 1.0f - (distance - inner) / (radius - inner);
				glow = glow * glow * (3.0f - 2.0f * glow);  // smoothstep
			}

			const int i = Index(cx, cy);
			m_Live[i] = std::max(m_Live[i], glow);

			// 등불이 충분히 비춘 칸만 발자국으로 기억한다.
			// 광원 가장자리의 희미한 부분까지 기억하면 발자국이 지나치게 굵어진다.
			if (leavesTrail && glow > 0.35f)
			{
				m_Trail[i] = 1.0f;
			}
		}
	}
}

void LightGrid::Decay(float deltaSeconds)
{
	if (m_TrailLife <= 0.0f)
	{
		return;
	}

	const float step = deltaSeconds / m_TrailLife;
	for (float& remaining : m_Trail)
	{
		if (remaining > 0.0f)
		{
			remaining = std::max(0.0f, remaining - step);
		}
	}
}

float LightGrid::Brightness(int cellX, int cellY) const
{
	if (!InBounds(cellX, cellY))
	{
		return 0.0f;
	}

	const int i = Index(cellX, cellY);

	// 잔여 수명이 m_FadeTail 위에 있는 동안은 밝기를 유지하다가,
	// 그 아래로 내려가면 빠르게 0으로 떨어진다.
	const float trail = std::min(m_Trail[i] / m_FadeTail, 1.0f) * m_TrailCeiling;

	return std::max(trail, m_Live[i]);
}

float LightGrid::BrightnessAt(float worldX, float worldY) const
{
	return Brightness(static_cast<int>(worldX / m_CellSize),
	                  static_cast<int>(worldY / m_CellSize));
}

float LightGrid::Glow(int cellX, int cellY) const
{
	return InBounds(cellX, cellY) ? m_Live[Index(cellX, cellY)] : 0.0f;
}
