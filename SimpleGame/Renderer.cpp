#include "stdafx.h"
#include "Renderer.h"

#include "TextRaster.h"

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
	if (m_RectInstVbo != 0)   glDeleteBuffers(1, &m_RectInstVbo);
	if (m_ShapeInstVbo != 0)  glDeleteBuffers(1, &m_ShapeInstVbo);
	if (m_RectInstVao != 0)   glDeleteVertexArrays(1, &m_RectInstVao);
	if (m_ShapeInstVao != 0)  glDeleteVertexArrays(1, &m_ShapeInstVao);
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
	m_TextShader = CompileShaders("./Shaders/Text.vs", "./Shaders/Text.fs");

	// 모아 그리기용. 없어도 게임은 돌아가고, 예전처럼 하나씩 그린다.
	m_RectInstShader = CompileShaders("./Shaders/SolidRectInst.vs", "./Shaders/SolidRectInst.fs");
	m_ShapeInstShader = CompileShaders("./Shaders/ShapeInst.vs", "./Shaders/ShapeInst.fs");
	
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

	if (m_TextShader > 0)
	{
		m_TextAttribPosition = glGetAttribLocation(m_TextShader, "a_Position");
		m_TextLocCenter = glGetUniformLocation(m_TextShader, "u_Center");
		m_TextLocSize = glGetUniformLocation(m_TextShader, "u_Size");
		m_TextLocColor = glGetUniformLocation(m_TextShader, "u_Color");
		m_TextLocReveal = glGetUniformLocation(m_TextShader, "u_Reveal");
		m_TextLocMode = glGetUniformLocation(m_TextShader, "u_Mode");

		glUseProgram(m_TextShader);
		glUniform1i(glGetUniformLocation(m_TextShader, "u_Texture"), 0);
		glUniform2f(glGetUniformLocation(m_TextShader, "u_Viewport"),
		            static_cast<float>(m_WindowSizeX), static_cast<float>(m_WindowSizeY));
	}
	else
	{
		// 글자를 못 그려도 게임은 할 수 있다. 대사만 보이지 않는다.
		std::cout << "글자 셰이더를 불러오지 못해 대사가 표시되지 않습니다.\n";
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_SolidRectShader > 0 && m_VBORect > 0 && m_ShapeShader > 0 && m_VBOQuad > 0)
	{
		m_Initialized = true;
	}

	m_BatchReady = CreateBatchResources();
	if (!m_BatchReady)
	{
		std::cout << "모아 그리기를 쓸 수 없어 하나씩 그립니다. 드로우 콜이 많아집니다.\n";
	}

	// 배치용 VAO를 만들며 바인딩이 바뀌었다. 글자와 이미지는 기본 VAO를 쓴다.
	glBindVertexArray(m_VAO);
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

	if (IsBatching())
	{
		const float rgba[4] = { r, g, b, a };
		BeginKind(BatchKind::Rect);
		AppendRect(newX, newY, size, rgba, emissive);
		return;
	}

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
	Count(1, 1);
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

	if (IsBatching())
	{
		BeginKind(BatchKind::Shape);
		AppendShape(shape, centerX, centerY, halfWidth + padding, halfHeight + padding,
		            color, rotation, roundness, softness, emissive, halfWidth, halfHeight);
		return;
	}

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
	Count(1, 1);
}

const Renderer::TextSprite* Renderer::FindOrCreateText(const std::string& utf8, float pixelSize)
{
	if (utf8.empty() || m_TextShader == 0)
	{
		return NULL;
	}

	const std::string key = std::to_string(static_cast<int>(pixelSize * 10.0f)) + "|" + utf8;

	auto found = m_TextCache.find(key);
	if (found != m_TextCache.end())
	{
		return found->second.texture != 0 ? &found->second : NULL;
	}

	TextSprite sprite;
	std::vector<unsigned char> alpha;
	int width = 0;
	int height = 0;

	if (RasterizeTextLine(utf8, pixelSize, alpha, width, height))
	{
		glGenTextures(1, &sprite.texture);
		glBindTexture(GL_TEXTURE_2D, sprite.texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // 한 줄 폭이 4의 배수가 아니어도 어긋나지 않게
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, alpha.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		sprite.width = width;
		sprite.height = height;
	}

	// 실패한 문자열도 기록해, 매 프레임 다시 그리려 하지 않는다.
	const TextSprite& stored = m_TextCache.emplace(key, sprite).first->second;
	return stored.texture != 0 ? &stored : NULL;
}

float Renderer::MeasureLabel(const std::string& utf8, float pixelSize)
{
	const TextSprite* sprite = FindOrCreateText(utf8, pixelSize);
	return sprite != NULL ? static_cast<float>(sprite->width) : 0.0f;
}

float Renderer::DrawLabel(const std::string& utf8, float pixelSize, float left, float top,
                          const Color& color, float reveal)
{
	const TextSprite* sprite = FindOrCreateText(utf8, pixelSize);
	if (sprite == NULL || reveal <= 0.0f)
	{
		return sprite != NULL ? static_cast<float>(sprite->width) : 0.0f;
	}

	// 글자는 셰이더도 텍스처도 다르므로 모아 그릴 수 없다.
	// 쌓여 있던 것을 먼저 내보내야 앞뒤 순서가 지켜진다.
	Flush();

	// 픽셀 경계에 맞춰야 글자가 번지지 않는다.
	const float x = std::floor(left + 0.5f);
	const float y = std::floor(top + 0.5f);

	float centerX, centerY;
	GetGLPosition(x + sprite->width * 0.5f, y - sprite->height * 0.5f, &centerX, &centerY);

	glUseProgram(m_TextShader);
	glUniform2f(m_TextLocCenter, centerX, centerY);
	glUniform2f(m_TextLocSize, static_cast<float>(sprite->width), static_cast<float>(sprite->height));
	glUniform4f(m_TextLocColor, color.r, color.g, color.b, color.a);
	glUniform1f(m_TextLocReveal, reveal);
	glUniform1i(m_TextLocMode, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sprite->texture);

	glEnableVertexAttribArray(m_TextAttribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glVertexAttribPointer(m_TextAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_TextAttribPosition);
	glBindTexture(GL_TEXTURE_2D, 0);
	Count(1, 1);

	return static_cast<float>(sprite->width);
}

GLuint Renderer::CreateImage(int width, int height, bool smooth)
{
	GLuint image = 0;
	glGenTextures(1, &image);
	glBindTexture(GL_TEXTURE_2D, image);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return image;
}

void Renderer::UpdateImage(GLuint image, int width, int height, const unsigned char* rgba)
{
	if (image == 0)
	{
		return;
	}
	glBindTexture(GL_TEXTURE_2D, image);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::DrawImage(GLuint image, float left, float top, float width, float height, float alpha,
                         bool blocksGlow)
{
	if (image == 0 || m_TextShader == 0)
	{
		return;
	}

	Flush();

	float centerX, centerY;
	GetGLPosition(left + width * 0.5f, top - height * 0.5f, &centerX, &centerY);

	glUseProgram(m_TextShader);
	glUniform2f(m_TextLocCenter, centerX, centerY);
	glUniform2f(m_TextLocSize, width, height);
	glUniform4f(m_TextLocColor, 1.0f, 1.0f, 1.0f, alpha);
	glUniform1f(m_TextLocReveal, 1.0f);
	glUniform1i(m_TextLocMode, blocksGlow ? 1 : 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image);

	glEnableVertexAttribArray(m_TextAttribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glVertexAttribPointer(m_TextAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_TextAttribPosition);
	glBindTexture(GL_TEXTURE_2D, 0);
	Count(1, 1);
}

void Renderer::SetClip(float left, float bottom, float width, float height)
{
	// 쌓아 둔 것은 아직 자르기 전 영역의 것이다. 먼저 내보낸다.
	Flush();

	// 가위 테스트는 프레임버퍼 픽셀 단위이고 왼쪽 아래가 원점이다.
	// 창의 논리 크기와 실제 프레임버퍼 크기가 다를 수 있어 뷰포트로 환산한다.
	GLint viewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, viewport);
	const float scaleX = static_cast<float>(viewport[2]) / static_cast<float>(m_WindowSizeX);
	const float scaleY = static_cast<float>(viewport[3]) / static_cast<float>(m_WindowSizeY);

	const float pixelLeft = (left + m_WindowSizeX * 0.5f) * scaleX;
	const float pixelBottom = (bottom + m_WindowSizeY * 0.5f) * scaleY;

	glEnable(GL_SCISSOR_TEST);
	glScissor(static_cast<GLint>(std::floor(pixelLeft)), static_cast<GLint>(std::floor(pixelBottom)),
	          static_cast<GLsizei>(std::ceil(width * scaleX)), static_cast<GLsizei>(std::ceil(height * scaleY)));
}

void Renderer::ClearClip()
{
	Flush();

	// 켜둔 채로 다음 프레임에 넘어가면 후처리 버퍼를 비울 때도 잘린다.
	glDisable(GL_SCISSOR_TEST);
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
// ═══ 모아 그리기 ═══════════════════════════════════════════════════
//
// 사각형도 도형도 결국 같은 단위 사각형 여섯 꼭짓점을 그린다.
// 다른 것은 위치 · 크기 · 색뿐이므로, 그 값들만 따로 모아 인스턴스마다
// 하나씩 넘기면 수천 개를 glDrawArraysInstanced 한 번에 보낼 수 있다.
//
// glVertexAttribDivisor(loc, 1)이 "이 속성은 꼭짓점마다가 아니라
// 인스턴스마다 넘어간다"는 표시다. 이것을 VAO에 한 번 기록해 두면
// 그릴 때는 버퍼만 채우고 부르면 된다.

bool Renderer::CreateBatchResources()
{
	if (m_RectInstShader == 0 || m_ShapeInstShader == 0)
	{
		return false;
	}
	if (m_VBORect == 0 || m_VBOQuad == 0)
	{
		return false;
	}

	// ── 사각형 ──
	{
		const GLint aPos   = glGetAttribLocation(m_RectInstShader, "a_Position");
		const GLint iTrans = glGetAttribLocation(m_RectInstShader, "i_Trans");
		const GLint iColor = glGetAttribLocation(m_RectInstShader, "i_Color");
		const GLint iEmis  = glGetAttribLocation(m_RectInstShader, "i_Emissive");
		if (aPos < 0 || iTrans < 0 || iColor < 0 || iEmis < 0)
		{
			return false;
		}

		glGenVertexArrays(1, &m_RectInstVao);
		glBindVertexArray(m_RectInstVao);

		glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
		glEnableVertexAttribArray(aPos);
		glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

		glGenBuffers(1, &m_RectInstVbo);
		glBindBuffer(GL_ARRAY_BUFFER, m_RectInstVbo);

		const GLsizei stride = static_cast<GLsizei>(sizeof(float) * 9);
		glEnableVertexAttribArray(iTrans);
		glVertexAttribPointer(iTrans, 4, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 0));
		glVertexAttribDivisor(iTrans, 1);

		glEnableVertexAttribArray(iColor);
		glVertexAttribPointer(iColor, 4, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 4));
		glVertexAttribDivisor(iColor, 1);

		glEnableVertexAttribArray(iEmis);
		glVertexAttribPointer(iEmis, 1, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 8));
		glVertexAttribDivisor(iEmis, 1);
	}

	// ── 도형 ──
	{
		const GLint aPos    = glGetAttribLocation(m_ShapeInstShader, "a_Position");
		const GLint iCenter = glGetAttribLocation(m_ShapeInstShader, "i_CenterExtent");
		const GLint iColor  = glGetAttribLocation(m_ShapeInstShader, "i_Color");
		const GLint iParams = glGetAttribLocation(m_ShapeInstShader, "i_Params");
		const GLint iHalf   = glGetAttribLocation(m_ShapeInstShader, "i_HalfSoft");
		if (aPos < 0 || iCenter < 0 || iColor < 0 || iParams < 0 || iHalf < 0)
		{
			return false;
		}

		glGenVertexArrays(1, &m_ShapeInstVao);
		glBindVertexArray(m_ShapeInstVao);

		glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
		glEnableVertexAttribArray(aPos);
		glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

		glGenBuffers(1, &m_ShapeInstVbo);
		glBindBuffer(GL_ARRAY_BUFFER, m_ShapeInstVbo);

		const GLsizei stride = static_cast<GLsizei>(sizeof(float) * 15);
		glEnableVertexAttribArray(iCenter);
		glVertexAttribPointer(iCenter, 4, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 0));
		glVertexAttribDivisor(iCenter, 1);

		glEnableVertexAttribArray(iColor);
		glVertexAttribPointer(iColor, 4, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 4));
		glVertexAttribDivisor(iColor, 1);

		glEnableVertexAttribArray(iParams);
		glVertexAttribPointer(iParams, 4, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 8));
		glVertexAttribDivisor(iParams, 1);

		glEnableVertexAttribArray(iHalf);
		glVertexAttribPointer(iHalf, 3, GL_FLOAT, GL_FALSE, stride,
		                      reinterpret_cast<const void*>(sizeof(float) * 12));
		glVertexAttribDivisor(iHalf, 1);

		// 창 크기는 바뀌지 않으므로 한 번만 보낸다.
		glUseProgram(m_ShapeInstShader);
		glUniform2f(glGetUniformLocation(m_ShapeInstShader, "u_Viewport"),
		            static_cast<float>(m_WindowSizeX), static_cast<float>(m_WindowSizeY));
	}

	// 넉넉히 잡아 두면 첫 몇 프레임에 버퍼가 자라느라 멈칫하지 않는다.
	m_RectBatch.reserve(4096 * 9);
	m_ShapeBatch.reserve(4096 * 15);
	return true;
}

void Renderer::SetTag(DrawTag tag)
{
	m_Tag = tag;
}

void Renderer::SetBatching(bool enabled)
{
	if (m_Batching == enabled)
	{
		return;
	}
	Flush();
	m_Batching = enabled;
}

void Renderer::Count(int calls, int instances)
{
	m_DrawCount += calls;
	const int tag = static_cast<int>(m_Tag);
	m_Sample.calls[tag] += calls;
	m_Sample.instances[tag] += instances;
}

void Renderer::BeginKind(BatchKind kind)
{
	if (m_Kind != kind)
	{
		if (m_Kind != BatchKind::None)
		{
			// 종류가 바뀌면 끊을 수밖에 없다. 이 횟수가 곧 모아 그리기의 한계다.
			++m_Sample.kindBreaks;
		}
		Flush();
		m_Kind = kind;
		m_BatchTag = m_Tag;
	}
}

void Renderer::AppendRect(float x, float y, float size, const float rgba[4], float emissive)
{
	m_RectBatch.push_back(x);
	m_RectBatch.push_back(y);
	m_RectBatch.push_back(size);
	m_RectBatch.push_back(0.0f);          // 자리 맞춤. vec4 하나로 읽는다
	m_RectBatch.push_back(rgba[0]);
	m_RectBatch.push_back(rgba[1]);
	m_RectBatch.push_back(rgba[2]);
	m_RectBatch.push_back(rgba[3]);
	m_RectBatch.push_back(emissive);

	++m_Sample.instances[static_cast<int>(m_Tag)];
}

void Renderer::AppendShape(int shape, float centerX, float centerY, float extentX, float extentY,
                           const Color& color, float rotation, float roundness, float softness,
                           float emissive, float halfWidth, float halfHeight)
{
	m_ShapeBatch.push_back(centerX);
	m_ShapeBatch.push_back(centerY);
	m_ShapeBatch.push_back(extentX);
	m_ShapeBatch.push_back(extentY);

	m_ShapeBatch.push_back(color.r);
	m_ShapeBatch.push_back(color.g);
	m_ShapeBatch.push_back(color.b);
	m_ShapeBatch.push_back(color.a);

	m_ShapeBatch.push_back(rotation);
	m_ShapeBatch.push_back(emissive);
	m_ShapeBatch.push_back(static_cast<float>(shape));
	m_ShapeBatch.push_back(roundness);

	m_ShapeBatch.push_back(halfWidth);
	m_ShapeBatch.push_back(halfHeight);
	m_ShapeBatch.push_back(softness);

	++m_Sample.instances[static_cast<int>(m_Tag)];
}

// 쌓인 것을 GPU로 올린다. 버퍼를 통째로 버리고 새로 담는 것(orphaning)은
// 드라이버가 지난 프레임의 그리기가 끝나기를 기다리지 않게 하려는 것이다.
static void UploadInstances(const std::vector<float>& data, size_t& capacityBytes)
{
	const size_t bytes = data.size() * sizeof(float);
	if (bytes > capacityBytes)
	{
		glBufferData(GL_ARRAY_BUFFER, bytes, data.data(), GL_STREAM_DRAW);
		capacityBytes = bytes;
	}
	else
	{
		glBufferData(GL_ARRAY_BUFFER, capacityBytes, NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data.data());
	}
}

void Renderer::FlushRects()
{
	const GLsizei count = static_cast<GLsizei>(m_RectBatch.size() / 9);
	if (count <= 0)
	{
		return;
	}

	glBindVertexArray(m_RectInstVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_RectInstVbo);
	UploadInstances(m_RectBatch, m_RectInstCapacity);

	glUseProgram(m_RectInstShader);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);

	++m_DrawCount;
	++m_Sample.flushes;
	++m_Sample.calls[static_cast<int>(m_BatchTag)];

	m_RectBatch.clear();
	glBindVertexArray(m_VAO);
}

void Renderer::FlushShapes()
{
	const GLsizei count = static_cast<GLsizei>(m_ShapeBatch.size() / 15);
	if (count <= 0)
	{
		return;
	}

	glBindVertexArray(m_ShapeInstVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_ShapeInstVbo);
	UploadInstances(m_ShapeBatch, m_ShapeInstCapacity);

	glUseProgram(m_ShapeInstShader);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);

	++m_DrawCount;
	++m_Sample.flushes;
	++m_Sample.calls[static_cast<int>(m_BatchTag)];

	m_ShapeBatch.clear();
	glBindVertexArray(m_VAO);
}

void Renderer::Flush()
{
	if (!m_RectBatch.empty())
	{
		FlushRects();
	}
	if (!m_ShapeBatch.empty())
	{
		FlushShapes();
	}
	m_Kind = BatchKind::None;
}
