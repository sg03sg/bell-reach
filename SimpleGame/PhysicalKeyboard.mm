#include "PhysicalKeyboard.h"

#import <AppKit/AppKit.h>

namespace
{
	KeyHandler g_Down = nullptr;
	KeyHandler g_Up = nullptr;
	id g_Monitor = nil;   // 붙들고 있어야 모니터가 살아 있다

	ScrollHandler g_Scroll = nullptr;
	id g_ScrollMonitor = nil;

	//
	// macOS 가상 키 코드 → 영문 소문자.
	// 이 값들은 키보드 위의 자리를 가리키며 입력 소스와 상관없다
	// (Carbon의 kVK_ANSI_* 와 같은 값).
	//
	unsigned char ToAscii(unsigned short keyCode)
	{
		switch (keyCode)
		{
		case 0x00: return 'a';
		case 0x0B: return 'b';
		case 0x08: return 'c';
		case 0x02: return 'd';
		case 0x0E: return 'e';
		case 0x03: return 'f';
		case 0x05: return 'g';
		case 0x04: return 'h';
		case 0x22: return 'i';
		case 0x26: return 'j';
		case 0x28: return 'k';
		case 0x25: return 'l';
		case 0x2E: return 'm';
		case 0x2D: return 'n';
		case 0x1F: return 'o';
		case 0x23: return 'p';
		case 0x0C: return 'q';
		case 0x0F: return 'r';
		case 0x01: return 's';
		case 0x11: return 't';
		case 0x20: return 'u';
		case 0x09: return 'v';
		case 0x0D: return 'w';
		case 0x07: return 'x';
		case 0x10: return 'y';
		case 0x06: return 'z';
		case 0x31: return ' ';
		case 0x30: return '\t';   // Tab — 누르고 있는 동안 상태 창
		case 0x12: return '1';     // 숫자 — 거래 창에서 고르고, 평소에는 단축칸을 쓴다
		case 0x13: return '2';
		case 0x14: return '3';
		case 0x15: return '4';
		case 0x17: return '5';
		case 0x16: return '6';
		case 0x1A: return '7';
		case 0x1C: return '8';
		case 0x19: return '9';
		case 0x1D: return '0';
		case 0x18: return '=';     // 큰 지도 확대
		case 0x1B: return '-';     // 큰 지도 축소
		case 0x35: return 27;    // ESC
		default:   return 0;     // 방향키 같은 나머지는 GLUT가 그대로 맡는다
		}
	}
}

bool InstallPhysicalKeyboard(KeyHandler down, KeyHandler up)
{
	if (g_Monitor != nil)
	{
		return true;
	}

	g_Down = down;
	g_Up = up;

	g_Monitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp)
	                                                  handler:^NSEvent* (NSEvent* event)
	{
		// ⌘Q 같은 메뉴 단축키는 건드리지 않는다. 삼키면 게임을 끌 수 없게 된다.
		if ((event.modifierFlags & NSEventModifierFlagCommand) != 0)
		{
			return event;
		}

		const unsigned char key = ToAscii(event.keyCode);
		if (key == 0)
		{
			return event;
		}

		if (event.type == NSEventTypeKeyDown)
		{
			// 누르고 있을 때 반복해 들어오는 것은 버린다(GLUT의 glutIgnoreKeyRepeat와 같다).
			if (!event.isARepeat && g_Down != nullptr)
			{
				g_Down(key, 0, 0);
			}
		}
		else if (g_Up != nullptr)
		{
			g_Up(key, 0, 0);
		}

		// 삼킨다. 입력기에도 GLUT에도 가지 않는다.
		return nil;
	}];

	return g_Monitor != nil;
}

bool InstallScrollWheel(ScrollHandler handler)
{
	if (g_ScrollMonitor != nil)
	{
		return true;
	}

	g_Scroll = handler;
	g_ScrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
	                                                        handler:^NSEvent* (NSEvent* event)
	{
		if (g_Scroll != nullptr)
		{
			// 트랙패드는 픽셀 단위로 잘게, 마우스 휠은 한 칸씩 온다. 휠 한 칸을 1로 맞춘다.
			const float amount = event.hasPreciseScrollingDeltas
				? static_cast<float>(event.scrollingDeltaY) / 12.0f
				: static_cast<float>(event.scrollingDeltaY);
			if (amount != 0.0f)
			{
				g_Scroll(amount);
			}
		}
		return event;
	}];

	return g_ScrollMonitor != nil;
}
