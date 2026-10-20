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

	// 거래 창에서 고르는 숫자(1~4)
	bool Choice(int number) const
	{
		return number >= 1 && number <= 9 && m_Ascii['0' + number];
	}

	bool IsPressed(unsigned char key) const;

private:
	bool m_Ascii[256] = { false };
	bool m_Special[256] = { false };
};
