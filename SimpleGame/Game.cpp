#include "stdafx.h"
#include "Game.h"

#include "Input.h"
#include "Renderer.h"

#include <cmath>

namespace
{
	// 타일 한 칸의 픽셀 크기. 맵은 30 x 20 타일이므로 창은 960 x 640이 된다.
	const int   kTileSize   = 32;
	const int   kMapCols    = 30;
	const int   kMapRows    = 20;

	// 밝기 격자는 타일보다 촘촘하게 잡는다. 조명이 타일 단위로 각지지 않는다.
	const float kLightCell  = 16.0f;

	// 어둠에 완전히 잠긴 칸은 배경색과 같으므로 아예 그리지 않는다.
	// 화면 대부분이 어둠이라, 이 컷 하나로 드로우 콜이 한 자릿수 비율로 줄어든다.
	const float kVisibleCut = 0.02f;

	// 맵. '#'은 벽, 그 외는 바닥.
	// 지금은 코드에 박혀 있지만, v0.2에서 파일로 빼는 것이 로드맵의 분수령이다.
	const char* const kMap[kMapRows] =
	{
		"##############################",
		"#............................#",
		"#..####..........####........#",
		"#..#..#..........#..#........#",
		"#..#..#..........#..#........#",
		"#..####..........####........#",
		"#............................#",
		"#......##########............#",
		"#......#........#............#",
		"#......#........#............#",
		"#......#........#............#",
		"#......####..####............#",
		"#............................#",
		"#....######........######....#",
		"#.........#........#.........#",
		"#.........#........#.........#",
		"#.........##########.........#",
		"#............................#",
		"#............................#",
		"##############################",
	};

	// 어둠 속에서 드러나는 색들. 채도를 낮게 잡아, 등불의 호박색만 따뜻하게 남긴다.
	const float kFloorColor[3] = { 0.26f, 0.28f, 0.32f };
	const float kWallColor[3]  = { 0.08f, 0.09f, 0.13f };
	const float kFlame[3]      = { 0.98f, 0.72f, 0.36f };

	float Mix(float a, float b, float t)
	{
		return a + (b - a) * t;
	}
}

void Game::Initialize(int windowWidth, int windowHeight)
{
	m_WindowWidth = windowWidth;
	m_WindowHeight = windowHeight;

	m_Light.Initialize(static_cast<int>(windowWidth / kLightCell),
	                   static_cast<int>(windowHeight / kLightCell),
	                   kLightCell);

	// 가운데 열린 통로에서 시작한다.
	m_PlayerX = 15 * kTileSize + kTileSize * 0.5f;
	m_PlayerY = 12 * kTileSize + kTileSize * 0.5f;

	// 화톳불 두 개. 설치형 광원은 감쇠하지 않고 계속 그 자리를 밝힌다.
	m_StaticLights.push_back({ 20 * kTileSize + 16.0f,  6 * kTileSize + 16.0f, 110.0f });
	m_StaticLights.push_back({  8 * kTileSize + 16.0f, 18 * kTileSize + 16.0f, 110.0f });
}

bool Game::IsWallTile(int tileX, int tileY) const
{
	if (tileX < 0 || tileX >= kMapCols || tileY < 0 || tileY >= kMapRows)
	{
		return true;
	}
	return kMap[tileY][tileX] == '#';
}

bool Game::IsWallAtWorld(float worldX, float worldY) const
{
	return IsWallTile(static_cast<int>(worldX / kTileSize),
	                  static_cast<int>(worldY / kTileSize));
}

bool Game::IsPlayerBlocked(float worldX, float worldY) const
{
	// 플레이어 사각형의 네 모서리를 검사한다.
	// 사각형끼리의 판정이라 렌더러가 그리는 것과 같은 수학이다.
	const float h = m_PlayerHalfSize;
	return IsWallAtWorld(worldX - h, worldY - h)
	    || IsWallAtWorld(worldX + h, worldY - h)
	    || IsWallAtWorld(worldX - h, worldY + h)
	    || IsWallAtWorld(worldX + h, worldY + h);
}

void Game::Update(float deltaSeconds, const Input& input)
{
	float moveX = 0.0f;
	float moveY = 0.0f;

	if (input.MoveLeft())  { moveX -= 1.0f; }
	if (input.MoveRight()) { moveX += 1.0f; }
	if (input.MoveUp())    { moveY -= 1.0f; }
	if (input.MoveDown())  { moveY += 1.0f; }

	// 대각선이 더 빠르지 않도록 정규화한다.
	const float length = std::sqrt(moveX * moveX + moveY * moveY);
	if (length > 0.0f)
	{
		const float step = m_PlayerSpeed * deltaSeconds;
		const float dx = moveX / length * step;
		const float dy = moveY / length * step;

		// 축을 나눠서 이동해야 벽에 부딪혔을 때 미끄러진다.
		if (!IsPlayerBlocked(m_PlayerX + dx, m_PlayerY)) { m_PlayerX += dx; }
		if (!IsPlayerBlocked(m_PlayerX, m_PlayerY + dy)) { m_PlayerY += dy; }
	}

	m_Light.BeginFrame();

	// 등불만 발자국을 남긴다. 화톳불은 그 자리에 계속 있으므로 기억할 필요가 없다.
	m_Light.AddLight(m_PlayerX, m_PlayerY, m_LanternRadius, true);
	for (const StaticLight& fire : m_StaticLights)
	{
		m_Light.AddLight(fire.x, fire.y, fire.radius, false);
	}

	m_Light.Decay(deltaSeconds);
}

float Game::PlayerBrightness() const
{
	return m_Light.BrightnessAt(m_PlayerX, m_PlayerY);
}

void Game::Render(Renderer* renderer)
{
	m_DrawnCells = 0;

	// 배경이 곧 어둠이다. 그래서 어둠을 덮어 그리는 대신,
	// 밝은 칸만 밝기를 곱해서 그린다. 알파 블렌딩도, 오버레이도 필요 없다.
	for (int cy = 0; cy < m_Light.Rows(); ++cy)
	{
		for (int cx = 0; cx < m_Light.Cols(); ++cx)
		{
			const float brightness = m_Light.Brightness(cx, cy);
			if (brightness < kVisibleCut)
			{
				continue;
			}

			const float worldX = (cx + 0.5f) * kLightCell;
			const float worldY = (cy + 0.5f) * kLightCell;

			const bool wall = IsWallAtWorld(worldX, worldY);
			const float* base = wall ? kWallColor : kFloorColor;

			// 등불이 직접 비추는 곳은 따뜻하게, 남은 발자국은 차갑게 식어간다.
			const float warmth = m_Light.Glow(cx, cy) * 0.35f;

			renderer->DrawSolidRect(
				ToRenderX(worldX), ToRenderY(worldY), 0.0f, kLightCell,
				Mix(base[0], kFlame[0], warmth) * brightness,
				Mix(base[1], kFlame[1], warmth) * brightness,
				Mix(base[2], kFlame[2], warmth) * brightness,
				1.0f);

			++m_DrawnCells;
		}
	}

	// 화톳불의 불꽃
	for (const StaticLight& fire : m_StaticLights)
	{
		renderer->DrawSolidRect(ToRenderX(fire.x), ToRenderY(fire.y), 0.0f, 10.0f,
		                        kFlame[0], kFlame[1], kFlame[2], 1.0f);
	}

	// 플레이어. 등불을 든 당사자이므로 항상 보인다.
	renderer->DrawSolidRect(ToRenderX(m_PlayerX), ToRenderY(m_PlayerY), 0.0f,
	                        m_PlayerHalfSize * 2.0f,
	                        0.92f, 0.78f, 0.52f, 1.0f);
}
