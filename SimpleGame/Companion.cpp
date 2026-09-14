#include "stdafx.h"
#include "Companion.h"

#include <algorithm>
#include <cmath>

void Companion::Wake(float worldX, float worldY, int regionX, int regionY)
{
	m_Awake = true;
	m_X = worldX;
	m_Y = worldY;
	m_Darkness = 0.0f;
	m_RegionX = regionX;
	m_RegionY = regionY;
}

void Companion::Dismiss()
{
	m_Awake = false;
	m_Darkness = 0.0f;
}

void Companion::MoveTowards(float targetX, float targetY, float deltaSeconds)
{
	if (!m_Awake || IsFrozenAgain())
	{
		// 완전히 굳으면 그 자리에 멈춘다. 죽는 것이 아니라,
		// 플레이어가 돌아와 다시 말을 걸어야 한다.
		return;
	}

	const float dx = targetX - m_X;
	const float dy = targetY - m_Y;
	const float distance = std::sqrt(dx * dx + dy * dy);

	if (distance < 0.5f)
	{
		return;
	}

	// 굳어갈수록 발이 무거워진다. 방치하면 점점 뒤처진다.
	const float sluggish = 1.0f - m_Darkness * 0.45f;

	// 궤적을 따라가는 것이므로 목표점에 바로 붙어도 되지만,
	// 속도 상한을 두면 화면에서 걷는 것처럼 보인다.
	float speed = 210.0f * sluggish;

	// 너무 벌어졌으면 뛴다. 문틈이나 모퉁이에서 뒤처지는 것을 막는다.
	if (distance > 90.0f)
	{
		speed *= 2.2f;
	}

	const float step = std::min(distance, speed * deltaSeconds);
	m_X += dx / distance * step;
	m_Y += dy / distance * step;
}

void Companion::UpdateErosion(float deltaSeconds, float brightness)
{
	if (!m_Awake)
	{
		return;
	}

	if (brightness >= SafeBrightness())
	{
		m_Darkness -= deltaSeconds / m_RecoverSeconds;
	}
	else
	{
		m_Darkness += deltaSeconds / m_ErodeSeconds;
	}

	m_Darkness = std::max(0.0f, std::min(1.0f, m_Darkness));
}
