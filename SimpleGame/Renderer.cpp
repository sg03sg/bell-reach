#include "stdafx.h"
#include "Renderer.h"

#include <climits>
#include <cmath>
#include <cstring>

namespace
{
	// Shape.fs의 u_Shape 값과 맞춘다.
	const int kShapeRoundRect = 0;
	const int kShapeEllipse = 1;
	const int kShapeTriangle = 2;
}

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}


Renderer::~Renderer()
{
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	//Set window size
	m_WindowSizeX = windowSizeX;
	m_WindowSizeY = windowSizeY;

	// A bound Vertex Array Object is mandatory in an OpenGL Core Profile
	// context (the only kind macOS offers for GLSL 3.30).
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	//Load shaders
	m_SolidRectShader = CompileShaders("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_ShapeShader = CompileShaders("./Shaders/Shape.vs", "./Shaders/Shape.fs");
	
	//Create VBOs
	CreateVertexBufferObjects();

	if (m_SolidRectShader > 0)
	{
		m_LocTrans = glGetUniformLocation(m_SolidRectShader, "u_Trans");
		m_LocColor = glGetUniformLocation(m_SolidRectShader, "u_Color");
		m_AttribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
		m_LocEmissive = glGetUniformLocation(m_SolidRectShader, "u_Emissive");
	}

	if (m_ShapeShader > 0)
	{
		m_ShapeAttribPosition = glGetAttribLocation(m_ShapeShader, "a_Position");
		m_ShapeLocCenter = glGetUniformLocation(m_ShapeShader, "u_Center");
		m_ShapeLocExtent = glGetUniformLocation(m_ShapeShader, "u_Extent");
		m_ShapeLocRotation = glGetUniformLocation(m_ShapeShader, "u_Rotation");
		m_ShapeLocColor = glGetUniformLocation(m_ShapeShader, "u_Color");
		m_ShapeLocEmissive = glGetUniformLocation(m_ShapeShader, "u_Emissive");
		m_ShapeLocShape = glGetUniformLocation(m_ShapeShader, "u_Shape");
		m_ShapeLocHalfSize = glGetUniformLocation(m_ShapeShader, "u_HalfSize");
		m_ShapeLocRoundness = glGetUniformLocation(m_ShapeShader, "u_Roundness");
		m_ShapeLocSoftness = glGetUniformLocation(m_ShapeShader, "u_Softness");

		// 창 크기는 바뀌지 않으므로 한 번만 보낸다.
		glUseProgram(m_ShapeShader);
		glUniform2f(glGetUniformLocation(m_ShapeShader, "u_Viewport"),
		            static_cast<float>(m_WindowSizeX), static_cast<float>(m_WindowSizeY));
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_SolidRectShader > 0 && m_VBORect > 0 && m_ShapeShader > 0 && m_VBOQuad > 0)
	{
		m_Initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return m_Initialized;
}

void Renderer::CreateVertexBufferObjects()
{
	float rect[]
		=
	{
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, -1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, //Triangle1
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f,  1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);

	// 도형용 단위 사각형. 크기와 위치는 셰이더가 uniform으로 정한다.
	const float quad[] =
	{
		-0.5f, -0.5f,   0.5f, -0.5f,   0.5f,  0.5f,
		-0.5f, -0.5f,   0.5f,  0.5f,  -0.5f,  0.5f,
	};

	glGenBuffers(1, &m_VBOQuad);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
}

void Renderer::AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	//쉐이더 오브젝트 생성
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
	}

	const GLchar* p[1];
	p[0] = pShaderText;
	GLint Lengths[1];

	size_t slen = strlen(pShaderText);
	if (slen > INT_MAX) {
		// Handle error
	}
	GLint len = (GLint)slen;

	Lengths[0] = len;
	//쉐이더 코드를 쉐이더 오브젝트에 할당
	glShaderSource(ShaderObj, 1, p, Lengths);

	//할당된 쉐이더 코드를 컴파일
	glCompileShader(ShaderObj);

	GLint success;
	// ShaderObj 가 성공적으로 컴파일 되었는지 확인
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];

		//OpenGL 의 shader log 데이터를 가져옴
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		printf("%s \n", pShaderText);
	}

	// ShaderProgram 에 attach!!
	glAttachShader(ShaderProgram, ShaderObj);
}

bool Renderer::ReadFile(const char* filename, std::string *target)
{
	std::ifstream file(filename);
	if (file.fail())
	{
		std::cout << filename << " file loading failed.. \n";
		file.close();
		return false;
	}
	std::string line;
	while (getline(file, line)) {
		target->append(line.c_str());
		target->append("\n");
	}
	return true;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	GLuint ShaderProgram = glCreateProgram(); //빈 쉐이더 프로그램 생성

	if (ShaderProgram == 0) { //쉐이더 프로그램이 만들어졌는지 확인
		fprintf(stderr, "Error creating shader program\n");
	}

	std::string vs, fs;

	//shader.vs 가 vs 안으로 로딩됨
	if (!ReadFile(filenameVS, &vs)) {
		printf("Error compiling vertex shader\n");
		return 0;
	};

	//shader.fs 가 fs 안으로 로딩됨
	if (!ReadFile(filenameFS, &fs)) {
		printf("Error compiling fragment shader\n");
		return 0;
	};

	// ShaderProgram 에 vs.c_str() 버텍스 쉐이더를 컴파일한 결과를 attach함
	AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);

	// ShaderProgram 에 fs.c_str() 프레그먼트 쉐이더를 컴파일한 결과를 attach함
	AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };

	//Attach 완료된 shaderProgram 을 링킹함
	glLinkProgram(ShaderProgram);

	//링크가 성공했는지 확인
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);

	if (Success == 0) {
		// shader program 로그를 받아옴
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << ErrorLog;
		return 0;
	}

	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << ErrorLog;
		return 0;
	}

	glUseProgram(ShaderProgram);
	std::cout << filenameVS << ", " << filenameFS << " Shader compiling is done.\n";

	return ShaderProgram;
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	DrawRect(x, y, size, r, g, b, a, 0.0f);
}

void Renderer::DrawEmissiveRect(float x, float y, float z, float size, float r, float g, float b, float a,
                                float emissive)
{
	DrawRect(x, y, size, r, g, b, a, emissive);
}

void Renderer::DrawRect(float x, float y, float size, float r, float g, float b, float a, float emissive)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	glUseProgram(m_SolidRectShader);

	glUniform4f(m_LocTrans, newX, newY, 0, size);
	glUniform4f(m_LocColor, r, g, b, a);

	if (emissive != m_LastEmissive)
	{
		glUniform1f(m_LocEmissive, emissive);
		m_LastEmissive = emissive;
	}

	glEnableVertexAttribArray(m_AttribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glVertexAttribPointer(m_AttribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_AttribPosition);
	++m_DrawCount;
}

void Renderer::DrawShape(int shape, float x, float y, float halfWidth, float halfHeight, const Color& color,
                         float rotation, float roundness, float softness, float emissive)
{
	if (halfWidth <= 0.0f || halfHeight <= 0.0f || color.a <= 0.0f)
	{
		return;
	}

	float centerX, centerY;
	GetGLPosition(x, y, &centerX, &centerY);

	// 경계를 부드럽게 섞을 여유. 이만큼 사각형을 크게 잡아야 가장자리가 잘리지 않는다.
	const float padding = softness + 2.0f;

	glUseProgram(m_ShapeShader);

	glUniform2f(m_ShapeLocCenter, centerX, centerY);
	glUniform2f(m_ShapeLocExtent, halfWidth + padding, halfHeight + padding);
	glUniform1f(m_ShapeLocRotation, rotation);
	glUniform4f(m_ShapeLocColor, color.r, color.g, color.b, color.a);
	glUniform1f(m_ShapeLocEmissive, emissive);
	glUniform1i(m_ShapeLocShape, shape);
	glUniform2f(m_ShapeLocHalfSize, halfWidth, halfHeight);
	glUniform1f(m_ShapeLocRoundness, roundness);
	glUniform1f(m_ShapeLocSoftness, softness);

	glEnableVertexAttribArray(m_ShapeAttribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glVertexAttribPointer(m_ShapeAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_ShapeAttribPosition);
	++m_DrawCount;
}

void Renderer::DrawCircle(float x, float y, float radius, const Color& color, float emissive, float softness)
{
	DrawShape(kShapeEllipse, x, y, radius, radius, color, 0.0f, 0.0f, softness, emissive);
}

void Renderer::DrawEllipse(float x, float y, float radiusX, float radiusY, const Color& color,
                           float rotation, float emissive, float softness)
{
	DrawShape(kShapeEllipse, x, y, radiusX, radiusY, color, rotation, 0.0f, softness, emissive);
}

void Renderer::DrawRoundRect(float x, float y, float width, float height, float roundness, const Color& color,
                             float rotation, float emissive)
{
	DrawShape(kShapeRoundRect, x, y, width * 0.5f, height * 0.5f, color, rotation, roundness, 0.0f, emissive);
}

void Renderer::DrawTriangle(float x, float y, float width, float height, const Color& color,
                            float rotation, float emissive)
{
	DrawShape(kShapeTriangle, x, y, width * 0.5f, height * 0.5f, color, rotation, 0.0f, 0.0f, emissive);
}

void Renderer::DrawSegment(float x0, float y0, float x1, float y1, float thickness, const Color& color,
                           float emissive)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float length = std::sqrt(dx * dx + dy * dy);

	// 양 끝이 둥근 막대. 둥근 사각형을 두 점 사이 방향으로 돌려 놓는다.
	DrawShape(kShapeRoundRect, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f,
	          (length + thickness) * 0.5f, thickness * 0.5f, color,
	          std::atan2(dy, dx), thickness * 0.5f, 0.0f, emissive);
}

void Renderer::GetGLPosition(float x, float y, float *newX, float *newY)
{
	*newX = x * 2.f / m_WindowSizeX;
	*newY = y * 2.f / m_WindowSizeY;
}