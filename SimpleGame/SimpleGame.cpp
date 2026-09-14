/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "Dependencies/GL_Platform.h"
#include "Dependencies/GLUT_Platform.h"

#include "Renderer.h"
#include "Game.h"
#include "Input.h"
#include "PostProcess.h"

// 맵이 30 x 20 타일, 타일 한 칸이 32픽셀이므로 창 크기가 결정된다.
const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 640;

Renderer* g_Renderer = NULL;
Game g_Game;
Input g_Input;
PostProcess g_Post;

int g_PreviousTimeMs = 0;

// ── 개발용 자동 캡처 ─────────────────────────────────────────────
// `SimpleGame --shot <경로>` 로 실행하면 정해진 경로를 걸어다닌 뒤
// 프레임버퍼를 PPM으로 저장하고 종료한다. 화면 기록 권한 없이도
// 렌더링 결과를 확인할 수 있어, 혼자 개발할 때 눈 역할을 한다.
bool g_ShotMode = false;
const char* g_ShotPath = NULL;
float g_ShotElapsed = 0.0f;

void SaveFramebuffer(const char* path)
{
	GLint viewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, viewport);
	const int width = viewport[2];
	const int height = viewport[3];

	std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	std::ofstream file(path, std::ios::binary);
	if (!file)
	{
		std::cout << "캡처 실패: " << path << "\n";
		return;
	}

	file << "P6\n" << width << " " << height << "\n255\n";

	// OpenGL은 아래에서 위로 읽으므로 뒤집어서 쓴다.
	for (int y = height - 1; y >= 0; --y)
	{
		file.write(reinterpret_cast<const char*>(&pixels[static_cast<size_t>(y) * width * 3]),
		           static_cast<std::streamsize>(width) * 3);
	}

	std::cout << "캡처 저장: " << path << " (" << width << "x" << height << ")\n";
}

// 캡처용으로 미리 정해둔 이동 경로. 발자국이 남는 것을 보이기 위한 것이다.
void DriveScriptedWalk(float elapsed)
{
	const unsigned char keys[4] = { 'd', 's', 'a', 'w' };
	const float until[4] = { 1.0f, 1.8f, 3.2f, 4.2f };

	for (int i = 0; i < 4; ++i)
	{
		g_Input.OnKeyUp(keys[i]);
	}

	for (int i = 0; i < 4; ++i)
	{
		if (elapsed < until[i])
		{
			g_Input.OnKeyDown(keys[i]);
			return;
		}
	}
}

// 콘솔 진단용
float g_FpsAccumulator = 0.0f;
int g_FpsFrames = 0;
int g_LastDrawCalls = 0;

void RenderScene(void)
{
	// 배경이 곧 연기다. Game의 안개 층이 이 위에 땅을 아주 어둡게 깐다.
	// 후처리의 비네트도 이 색으로 가라앉는다.
	g_Renderer->ResetDrawCount();
	g_Post.BeginScene(0.020f, 0.022f, 0.031f);

	g_Game.Render(g_Renderer);

	// 장면(HDR) -> 번짐 · 가장자리 흐림 · 톤 매핑 · 비네트 -> 화면
	g_Post.EndScene();

	// HUD는 후처리가 끝난 화면 위에. 비네트에 가려지지 않는다.
	g_Game.RenderHud(g_Renderer);
	g_LastDrawCalls = g_Renderer->DrawCount();

	if (g_ShotMode && g_ShotElapsed > 4.5f)
	{
		SaveFramebuffer(g_ShotPath);
		exit(0);
	}

	glutSwapBuffers();
}

void Idle(void)
{
	const int nowMs = glutGet(GLUT_ELAPSED_TIME);
	float deltaSeconds = (nowMs - g_PreviousTimeMs) / 1000.0f;
	g_PreviousTimeMs = nowMs;

	// 창을 옮기거나 디버거에 멈춰 있다가 돌아오면 큰 값이 튄다.
	// 그대로 두면 플레이어가 벽을 뚫고 순간이동한다.
	if (deltaSeconds > 0.1f)
	{
		deltaSeconds = 0.1f;
	}

	if (g_ShotMode)
	{
		g_ShotElapsed += deltaSeconds;
		DriveScriptedWalk(g_ShotElapsed);
	}

	g_Game.Update(deltaSeconds, g_Input);

	g_FpsAccumulator += deltaSeconds;
	++g_FpsFrames;
	if (g_FpsAccumulator >= 1.0f)
	{
		std::cout << "fps " << g_FpsFrames
		          << "  |  그리기 " << g_LastDrawCalls
		          << "  |  빛청크 " << g_Game.LitChunks()
		          << "  |  맵청크 " << g_Game.MapChunks()
		          << "  |  기름 " << static_cast<int>(g_Game.Oil() * 100.0f) << "%"
		          << "  |  Lv " << g_Game.Level()
		          << "  |  체력 " << static_cast<int>(g_Game.Health())
		          << "  |  적 " << g_Game.EnemyCount()
		          << "  |  울린 종 " << g_Game.RungRegions()
		          << "  |  동행 " << (g_Game.HasCompanion()
		                 ? (g_Game.CompanionDarkness() >= 1.0f ? "굳음"
		                    : (g_Game.CompanionDarkness() > 0.5f ? "위험" : "함께"))
		                 : "없음")
		          << "  |  위치 " << static_cast<int>(g_Game.PlayerX())
		          << ", " << static_cast<int>(g_Game.PlayerY())
		          << std::endl;
		g_FpsAccumulator = 0.0f;
		g_FpsFrames = 0;
	}

	glutPostRedisplay();
}

void MouseInput(int button, int state, int x, int y)
{
}

void KeyInput(unsigned char key, int x, int y)
{
	if (key == 27)  // ESC
	{
		exit(0);
	}

	// 후처리 전후를 바로 비교할 수 있게 한다.
	if (key == 'p' || key == 'P')
	{
		g_Post.SetEnabled(!g_Post.IsEnabled());
		std::cout << "후처리 " << (g_Post.IsEnabled() ? "켜짐" : "꺼짐") << "\n";
		return;
	}
	if (key == 'm' || key == 'M')
	{
		g_Game.ToggleMinimap();
		std::cout << "미니맵 " << (g_Game.IsMinimapVisible() ? "켜짐" : "꺼짐") << "\n";
		return;
	}
	if (key == 't' || key == 'T')
	{
		g_Post.settings.toneMapper = 1 - g_Post.settings.toneMapper;
		std::cout << "톤 매핑: " << (g_Post.settings.toneMapper == 1 ? "ACES" : "어깨 곡선") << "\n";
		return;
	}

	g_Input.OnKeyDown(key);
}

void KeyUpInput(unsigned char key, int x, int y)
{
	g_Input.OnKeyUp(key);
}

void SpecialKeyInput(int key, int x, int y)
{
	g_Input.OnSpecialKeyDown(key);
}

void SpecialKeyUpInput(int key, int x, int y)
{
	g_Input.OnSpecialKeyUp(key);
}

void Reshape(int width, int height)
{
	// 직접 등록하면 GLUT 기본 동작(뷰포트 갱신)이 사라지므로 대신 해 준다.
	glViewport(0, 0, width, height);
	g_Post.Resize(width, height);
}

int main(int argc, char **argv)
{
	// 섬은 실행할 때마다 새로 만들어진다. 같은 섬을 다시 보고 싶으면 --seed로 씨앗을 준다.
	unsigned int seed = static_cast<unsigned int>(std::time(NULL)) ^ static_cast<unsigned int>(std::clock() * 2654435761u);

	for (int i = 1; i < argc - 1; ++i)
	{
		if (std::strcmp(argv[i], "--shot") == 0)
		{
			g_ShotMode = true;
			g_ShotPath = argv[i + 1];
		}
		else if (std::strcmp(argv[i], "--seed") == 0)
		{
			seed = static_cast<unsigned int>(std::strtoul(argv[i + 1], NULL, 10));
		}
	}

	// Initialize GL things
	glutInit(&argc, argv);
#if defined(__APPLE__)
	// macOS only exposes GLSL 3.30+ through a Core Profile context.
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA | GLUT_3_2_CORE_PROFILE);
#else
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
#endif
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Bell Reach - Prototype M1");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << "GLEW Version is 3.0\n";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n";
	}

	// Initialize Renderer
	g_Renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";
		return 1;
	}

	g_Game.Initialize(WINDOW_WIDTH, WINDOW_HEIGHT, seed);
	std::cout << "섬의 씨앗: " << seed << "   (같은 섬을 다시 만들려면 --seed " << seed << ")\n";

	// 후처리 버퍼는 창의 논리 크기가 아니라 실제 프레임버퍼 크기로 만든다.
	GLint viewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, viewport);
	const int framebufferWidth = viewport[2] > 0 ? viewport[2] : WINDOW_WIDTH;
	const int framebufferHeight = viewport[3] > 0 ? viewport[3] : WINDOW_HEIGHT;
	g_Post.Initialize(g_Renderer, framebufferWidth, framebufferHeight);

	std::cout
		<< "\n  ── 조작 ─────────────────────────────────\n"
		<< "   WASD / 화살표   걷는다\n"
		<< "   F               불씨를 쏜다 — 가까운 적을 저절로 노린다 (누르고 있으면 연사)\n"
		<< "   Space           말을 건다 · 대사 넘기기 · 종을 당긴다(누르고 있을 것)\n"
		<< "   E               화톳불을 내려놓는다 (3개)\n"
		<< "   M               미니맵 켜기/끄기\n"
		<< "   P               후처리 켜기/끄기 (전후 비교)\n"
		<< "   T               톤 매핑 전환 (어깨 곡선 / ACES)\n"
		<< "   ESC             끝낸다\n"
		<< "\n  ── 한 바퀴 ──────────────────────────────\n"
		<< "   1. 연기 너머로 종탑과 굳은 사람이 희미하게 보인다.\n"
		<< "      까마귀가 앉아 있으면 아직 침묵한 종탑이다.\n"
		<< "   2. 굳은 사람 곁에 서서 등불을 2초쯤 비추면 색이 돌아온다.\n"
		<< "      등불은 내가 선 자리만 밝히므로, 떠나면 다시 식는다.\n"
		<< "   3. 색이 다 돌아와 머리 위에 불씨가 뜨면 Space로 말을 건다.\n"
		<< "      이야기를 끝까지 들으면 그가 종탑 방향을 알려 주고,\n"
		<< "      당신이 지나온 길을 따라오기 시작한다.\n"
		<< "      어둠 속에 오래 두면 다시 색이 빠지고 결국 멈춘다.\n"
		<< "   4. 둘이 함께 종탑에 붙어 Space를 끝까지 누르고 있으면\n"
		<< "      종이 울리고 그 일대가 영구히 밝아진다.\n"
		<< "      큰 종은 혼자 울릴 수 없다.\n"
		<< "\n  ── 싸움 ──────────────────────────────────\n"
		<< "   · 어둠 속에서 그림자들이 나타난다. 불빛 안에서는 발이 느려지고,\n"
		<< "     종을 울린 곳에는 들어오지 못한다.\n"
		<< "   · 쓰러진 자리에 떨어진 영혼석을 주우면 레벨이 오른다.\n"
		<< "     레벨이 오르면 체력, 불씨의 위력과 연사 속도, 사거리가 늘고\n"
		<< "     5레벨마다 불씨가 하나씩 더 나간다.\n"
		<< "   · 강화석은 무기를, 회복약은 체력을, 자석은 10초 동안 주변의 것을 모두 끌어온다.\n"
		<< "   · 종탑 앞의 종탑지기를 쓰러뜨려야 종을 울릴 수 있다.\n"
		<< "     종탑지기가 흩뿌리는 어둠 구슬은 집 벽과 나무에 막힌다.\n\n";

	// 키를 누르고 있을 때 GLUT가 KeyDown을 반복 발생시키지 않도록 한다.
	glutIgnoreKeyRepeat(1);

	glutDisplayFunc(RenderScene);
	glutReshapeFunc(Reshape);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);
	glutSpecialFunc(SpecialKeyInput);
	glutSpecialUpFunc(SpecialKeyUpInput);
	glutMouseFunc(MouseInput);

	g_PreviousTimeMs = glutGet(GLUT_ELAPSED_TIME);

	glutMainLoop();

	delete g_Renderer;

	return 0;
}
