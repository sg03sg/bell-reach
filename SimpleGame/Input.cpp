#include "stdafx.h"
#include "Input.h"

#include "Dependencies/GLUT_Platform.h"

#include <cctype>

void Input::OnKeyDown(unsigned char key)
{
	m_Ascii[key] = true;
	m_Ascii[std::tolower(key)] = true;
}

void Input::OnKeyUp(unsigned char key)
{
	m_Ascii[key] = false;
	m_Ascii[std::tolower(key)] = false;
}

void Input::OnSpecialKeyDown(int key)
{
	if (key >= 0 && key < 256)
	{
		m_Special[key] = true;
	}
}

void Input::OnSpecialKeyUp(int key)
{
	if (key >= 0 && key < 256)
	{
		m_Special[key] = false;
	}
}

bool Input::IsPressed(unsigned char key) const
{
	return m_Ascii[key];
}

bool Input::MoveLeft() const
{
	return m_Ascii['a'] || m_Special[GLUT_KEY_LEFT];
}

bool Input::MoveRight() const
{
	return m_Ascii['d'] || m_Special[GLUT_KEY_RIGHT];
}

bool Input::MoveUp() const
{
	return m_Ascii['w'] || m_Special[GLUT_KEY_UP];
}

bool Input::MoveDown() const
{
	return m_Ascii['s'] || m_Special[GLUT_KEY_DOWN];
}
