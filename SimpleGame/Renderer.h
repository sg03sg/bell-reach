#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies/GL_Platform.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void GetGLPosition(float x, float y, float *newX, float *newY);

	bool m_Initialized = false;
	
	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VAO = 0;
	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;

	// 밝기 격자를 매 프레임 수천 칸 그리므로, 이름으로 하는 위치 조회는
	// 초기화 때 한 번만 하고 캐시해 둔다.
	GLint m_LocTrans = -1;
	GLint m_LocColor = -1;
	GLint m_AttribPosition = -1;
};

