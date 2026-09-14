#pragma once

//
// 깨어난 동행자.
//
// 지켜야 할 짐이 아니라 함께 가는 사람이다. 다만 어둠 속에 오래 두면
// 다시 굳는다 — 이 게임에서 사람을 데리고 다닌다는 것의 대가다.
//
// 길찾기를 하지 않는다. 플레이어가 지나온 궤적 위의 한 점을 목표로 삼기
// 때문에, 이미 지나간 곳만 밟게 되어 벽에 끼지 않는다. A* 같은 것이
// 통째로 필요 없어지는 대신, 동행자는 플레이어가 간 길만 갈 수 있다.
//
class Companion
{
public:
	void Wake(float worldX, float worldY, int regionX, int regionY);
	void Dismiss();

	bool IsAwake() const { return m_Awake; }
	bool IsFrozenAgain() const { return m_Awake && m_Darkness >= 1.0f; }

	// 궤적에서 계산된 목표점으로 옮긴다. 너무 멀어지면 서둘러 따라온다.
	void MoveTowards(float targetX, float targetY, float deltaSeconds);

	// 밝은 곳에서는 회복하고 어두운 곳에서는 다시 굳어간다.
	void UpdateErosion(float deltaSeconds, float brightness);

	float X() const { return m_X; }
	float Y() const { return m_Y; }
	float Darkness() const { return m_Darkness; }
	int RegionX() const { return m_RegionX; }
	int RegionY() const { return m_RegionY; }

	// 이 밝기 아래에서는 어둠에 잠식된다. 오브젝트가 드러나는 문턱과 같은 값이라
	// "보이는 곳에 있으면 안전하다"가 한 가지 규칙으로 읽힌다.
	static float SafeBrightness() { return 0.30f; }

private:
	bool m_Awake = false;
	float m_X = 0.0f;
	float m_Y = 0.0f;

	// 0.0 = 완전히 깨어남, 1.0 = 다시 굳음.
	// 체력바 대신 이 값이 그대로 동행자의 색이 된다. UI가 필요 없다.
	float m_Darkness = 0.0f;

	int m_RegionX = 0;
	int m_RegionY = 0;

	// 어둠 속에서 완전히 굳기까지 40초, 빛 속에서 완전히 회복하기까지 8초.
	// 회복이 훨씬 빨라야 등불을 되돌려 비추는 행동이 보람 있다.
	float m_ErodeSeconds = 40.0f;
	float m_RecoverSeconds = 8.0f;
};
