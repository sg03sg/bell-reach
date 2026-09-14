#include "stdafx.h"
#include "LightGrid.h"

#include "World.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	long long PackKey(int chunkX, int chunkY)
	{
		return (static_cast<long long>(chunkY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(chunkX));
	}
}

void LightGrid::Configure(float cellSize)
{
	m_CellSize = cellSize;
	m_Chunks.clear();
}

int LightGrid::CellOf(float world) const
{
	return static_cast<int>(std::floor(world / m_CellSize));
}

const LightGrid::Chunk* LightGrid::Find(int chunkX, int chunkY) const
{
	auto found = m_Chunks.find(PackKey(chunkX, chunkY));
	return found == m_Chunks.end() ? NULL : &found->second;
}

LightGrid::Chunk& LightGrid::Touch(int chunkX, int chunkY)
{
	const long long key = PackKey(chunkX, chunkY);

	auto found = m_Chunks.find(key);
	if (found != m_Chunks.end())
	{
		return found->second;
	}

	Chunk chunk;
	chunk.trail.assign(kChunkCells * kChunkCells, 0.0f);
	chunk.live.assign(kChunkCells * kChunkCells, 0.0f);
	return m_Chunks.emplace(key, std::move(chunk)).first->second;
}

void LightGrid::BeginFrame()
{
	for (auto& entry : m_Chunks)
	{
		std::fill(entry.second.live.begin(), entry.second.live.end(), 0.0f);
	}
}

void LightGrid::AddLight(float worldX, float worldY, float radius, bool leavesTrail)
{
	if (radius <= 0.0f)
	{
		return;
	}

	// 광원이 닿는 칸만 훑는다.
	const int minCellX = CellOf(worldX - radius);
	const int maxCellX = CellOf(worldX + radius);
	const int minCellY = CellOf(worldY - radius);
	const int maxCellY = CellOf(worldY + radius);

	// 안쪽은 균일하게 밝고 바깥으로 갈수록 부드럽게 떨어진다.
	const float inner = radius * 0.55f;

	for (int cy = minCellY; cy <= maxCellY; ++cy)
	{
		for (int cx = minCellX; cx <= maxCellX; ++cx)
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
				glow = glow * glow * (3.0f - 2.0f * glow);   // smoothstep
			}

			Chunk& chunk = Touch(World::FloorDiv(cx, kChunkCells),
			                     World::FloorDiv(cy, kChunkCells));
			const int index = World::PositiveMod(cy, kChunkCells) * kChunkCells
			                + World::PositiveMod(cx, kChunkCells);

			chunk.live[index] = std::max(chunk.live[index], glow);

			// 등불이 충분히 비춘 칸만 발자국으로 기억한다.
			// 가장자리의 희미한 부분까지 기억하면 발자국이 지나치게 굵어진다.
			if (leavesTrail && glow > 0.35f)
			{
				chunk.trail[index] = 1.0f;
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

	for (auto it = m_Chunks.begin(); it != m_Chunks.end(); )
	{
		Chunk& chunk = it->second;
		bool anythingLeft = false;

		for (size_t i = 0; i < chunk.trail.size(); ++i)
		{
			if (chunk.trail[i] > 0.0f)
			{
				chunk.trail[i] = std::max(0.0f, chunk.trail[i] - step);
			}
			if (chunk.trail[i] > 0.0f || chunk.live[i] > 0.0f)
			{
				anythingLeft = true;
			}
		}

		// 완전히 식었고 지금 비추는 광원도 없으면 버린다.
		// 이 한 줄 덕분에 무한 맵을 아무리 걸어다녀도 메모리가 늘지 않는다.
		it = anythingLeft ? std::next(it) : m_Chunks.erase(it);
	}
}

float LightGrid::Brightness(int cellX, int cellY) const
{
	const Chunk* chunk = Find(World::FloorDiv(cellX, kChunkCells),
	                          World::FloorDiv(cellY, kChunkCells));
	if (chunk == NULL)
	{
		return 0.0f;
	}

	const int index = World::PositiveMod(cellY, kChunkCells) * kChunkCells
	                + World::PositiveMod(cellX, kChunkCells);

	// 잔여 수명이 m_FadeTail 위에 있는 동안은 밝기를 유지하다가,
	// 그 아래로 내려가면 빠르게 0으로 떨어진다.
	const float trail = std::min(chunk->trail[index] / m_FadeTail, 1.0f) * m_TrailCeiling;

	return std::max(trail, chunk->live[index]);
}

float LightGrid::BrightnessAt(float worldX, float worldY) const
{
	return Brightness(CellOf(worldX), CellOf(worldY));
}

float LightGrid::Glow(int cellX, int cellY) const
{
	const Chunk* chunk = Find(World::FloorDiv(cellX, kChunkCells),
	                          World::FloorDiv(cellY, kChunkCells));
	if (chunk == NULL)
	{
		return 0.0f;
	}

	return chunk->live[World::PositiveMod(cellY, kChunkCells) * kChunkCells
	                 + World::PositiveMod(cellX, kChunkCells)];
}
