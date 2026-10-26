#pragma once

//
// GLUT 콜백이 채우는 키 상태.
//
// GLUT는 "눌린 순간"과 "떼어진 순간"만 알려주기 때문에, 대각선 이동이나
// 키를 누르고 있는 동안의 지속 이동을 하려면 상태를 따로 들고 있어야 한다.
//
class Input
{
public:
	void OnKeyDown(unsigned char key);
	void OnKeyUp(unsigned char key);
	void OnSpecialKeyDown(int key);
	void OnSpecialKeyUp(int key);

	bool MoveLeft() const;
	bool MoveRight() const;
	bool MoveUp() const;
	bool MoveDown() const;

	// 말을 걸고, 종을 당긴다. 이 게임에서 사람과 관계를 맺는 유일한 키다.
	bool Interact() const;

	// 가까운 적에게 불씨를 쏜다. 누르고 있으면 쿨타임마다 계속 쏜다.
	bool Attack() const;

	// 화톳불을 내려놓는다.
	bool PlaceFire() const;

	// 누르고 있는 동안만 상태 창이 열린다
	bool HoldingStatus() const { return m_Ascii['\t']; }

	// 상인에게 산 것
	bool PlaceTorch() const { return m_Ascii['r']; }     // 횃불을 그 자리에 세운다
	bool UseHealPack() const { return m_Ascii['q']; }    // 회복약을 마신다

	// 숫자 키 — 거래 창에서는 살 것을, 평소에는 단축칸을 고른다
	bool Choice(int number) const
	{
		return number >= 1 && number <= 9 && m_Ascii['0' + number];
	}

	bool IsPressed(unsigned char key) const;

	// ── 마우스 ──
	// 좌표는 HUD와 같은 렌더러 좌표계다(화면 가운데가 원점, y가 위). 창 픽셀을
	// 바꾸는 일은 GLUT 콜백(SimpleGame.cpp)이 창 크기를 보고 한다.
	enum MouseButton { kMouseLeft = 0, kMouseRight = 1, kMouseButtons = 2 };

	void OnMouseButton(int button, bool down, float x, float y);
	void OnMouseMove(float x, float y);
	void OnWheel(float amount);   // 위로 굴리면 +

	float MouseX() const { return m_MouseX; }
	float MouseY() const { return m_MouseY; }
	bool MouseDown(int button) const
	{
		return button >= 0 && button < kMouseButtons && m_MouseDown[button];
	}
	// 지금까지 굴린 양의 합. 읽는 쪽이 지난번 값과의 차이를 쓴다
	// (Game은 const Input만 받으므로 여기서 비울 수 없다).
	float WheelTotal() const { return m_Wheel; }

private:
	bool m_Ascii[256] = { false };
	bool m_Special[256] = { false };

	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	bool m_MouseDown[kMouseButtons] = { false, false };
	float m_Wheel = 0.0f;
};
