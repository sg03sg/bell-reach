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

	bool IsPressed(unsigned char key) const;

private:
	bool m_Ascii[256] = { false };
	bool m_Special[256] = { false };
};
