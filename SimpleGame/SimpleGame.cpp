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
#include <cstring>
#include "Dependencies/GL_Platform.h"
#include "Dependencies/GLUT_Platform.h"

#include "Renderer.h"
#include "Game.h"
#include "Input.h"

// 맵이 30 x 20 타일, 타일 한 칸이 32픽셀이므로 창 크기가 결정된다.
const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 640;

Renderer* g_Renderer = NULL;
Game g_Game;
Input g_Input;

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

void RenderScene(void)
{
	// 배경이 곧 어둠이다. 밝혀지지 않은 곳은 이 색 그대로 남는다.
	glClearColor(0.030f, 0.035f, 0.048f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	g_Game.Render(g_Renderer);

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
		          << "  |  그려진 칸 " << g_Game.LastDrawnCells()
		          << "  |  발밑 밝기 " << g_Game.PlayerBrightness()
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

int main(int argc, char **argv)
{
	for (int i = 1; i < argc - 1; ++i)
	{
		if (std::strcmp(argv[i], "--shot") == 0)
		{
			g_ShotMode = true;
			g_ShotPath = argv[i + 1];
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

	g_Game.Initialize(WINDOW_WIDTH, WINDOW_HEIGHT);

	std::cout << "\n  이동: WASD 또는 화살표    종료: ESC\n"
	          << "  등불이 지나간 자리는 25초 동안 남았다가 마지막 5초에 빠르게 꺼진다.\n\n";

	// 키를 누르고 있을 때 GLUT가 KeyDown을 반복 발생시키지 않도록 한다.
	glutIgnoreKeyRepeat(1);

	glutDisplayFunc(RenderScene);
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
