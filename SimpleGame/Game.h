#pragma once

#include <vector>

#include "LightGrid.h"

class Renderer;
class Input;

//
// 프로토타입 단계의 게임 상태.
//
// 지금 담당하는 것은 마일스톤 1의 범위다 - 이동, 등불, 어둠 격자.
// 몬스터 / 동행자 / 이벤트는 아직 없다.
//
// 월드 좌표는 화면 왼쪽 위가 원점이고 y축은 아래로 증가한다.
// 렌더러는 화면 중앙이 원점이고 y축이 위로 증가하므로, 그리기 직전에만 변환한다.
//
class Game
{
public:
	struct StaticLight
	{
		float x;
		float y;
		float radius;
	};

	void Initialize(int windowWidth, int windowHeight);
	void Update(float deltaSeconds, const Input& input);
	void Render(Renderer* renderer);

	int LastDrawnCells() const { return m_DrawnCells; }
	float PlayerBrightness() const;

private:
	bool IsWallTile(int tileX, int tileY) const;
	bool IsWallAtWorld(float worldX, float worldY) const;
	bool IsPlayerBlocked(float worldX, float worldY) const;

	float ToRenderX(float worldX) const { return worldX - m_WindowWidth * 0.5f; }
	float ToRenderY(float worldY) const { return m_WindowHeight * 0.5f - worldY; }

	int m_WindowWidth = 0;
	int m_WindowHeight = 0;

	float m_PlayerX = 0.0f;
	float m_PlayerY = 0.0f;
	float m_PlayerSpeed = 190.0f;    // 픽셀/초
	float m_PlayerHalfSize = 9.0f;

	// 기획서 기준 등불 반경은 화면 폭의 1/6 정도. 한 화면을 다 보지 못한다.
	float m_LanternRadius = 155.0f;

	LightGrid m_Light;
	std::vector<StaticLight> m_StaticLights;

	int m_DrawnCells = 0;
};
