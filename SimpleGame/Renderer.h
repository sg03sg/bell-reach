#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "Dependencies/GL_Platform.h"
#include "DrawStats.h"

struct Color
{
	float r;
	float g;
	float b;
	float a;
};

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

	// 스스로 빛나는 사각형. 장면에는 r,g,b 그대로 그리고,
	// 발광 버퍼에 (r,g,b) * emissive 를 기록해 후처리 Bloom의 원본이 되게 한다.
	// 색은 1.0 이하로 두고 밝기는 emissive로만 올릴 것 — 그래야 불꽃 자체가
	// 흰 네모로 뭉개지지 않고, 빛은 번짐으로 표현된다.
	void DrawEmissiveRect(float x, float y, float z, float size, float r, float g, float b, float a,
	                      float emissive);

	// ── 도형 ─────────────────────────────────────────────────────
	// 거리 함수로 깎아 그리므로 경계가 매끄럽고, 크기와 회전이 자유롭다.
	// 좌표는 DrawSolidRect와 같이 화면 중앙 원점, y축 위쪽, 픽셀 단위다.
	// rotation은 라디안이며 반시계 방향이다.
	// emissive를 주면 발광 버퍼에도 기록되어 Bloom으로 번진다.
	// softness를 주면 경계가 그만큼 흐려진다(그림자용).
	void DrawCircle(float x, float y, float radius, const Color& color,
	                float emissive = 0.0f, float softness = 0.0f);
	void DrawEllipse(float x, float y, float radiusX, float radiusY, const Color& color,
	                 float rotation = 0.0f, float emissive = 0.0f, float softness = 0.0f);
	void DrawRoundRect(float x, float y, float width, float height, float roundness, const Color& color,
	                   float rotation = 0.0f, float emissive = 0.0f);

	// 위를 향한 삼각형. (x, y)는 삼각형을 감싸는 사각형의 중심이다.
	void DrawTriangle(float x, float y, float width, float height, const Color& color,
	                  float rotation = 0.0f, float emissive = 0.0f);

	// 두 점을 잇는 막대. 팔, 밧줄, 장작처럼 방향이 있는 것에 쓴다.
	void DrawSegment(float x0, float y0, float x1, float y1, float thickness, const Color& color,
	                 float emissive = 0.0f);

	// ── 글자 ─────────────────────────────────────────────────────
	// (left, top)을 왼쪽 위로 해서 한 줄을 그린다. 그린 폭(px)을 돌려준다.
	// 처음 보는 문자열은 운영체제 글꼴 엔진으로 한 번 그려 텍스처로 캐시한다.
	// reveal은 0~1. 왼쪽부터 그만큼만 보여 한 글자씩 적히는 효과를 낸다.
	// 이름을 DrawText로 하지 않은 것은 Windows 헤더에 같은 이름의 매크로가 있어서다.
	float DrawLabel(const std::string& utf8, float pixelSize, float left, float top,
	                const Color& color, float reveal = 1.0f);
	float MeasureLabel(const std::string& utf8, float pixelSize);

	// ── 이미지 ───────────────────────────────────────────────────
	// CPU에서 만든 RGBA 픽셀을 텍스처로 올려 그린다. 미니맵처럼 수천 칸을
	// 사각형으로 하나씩 그리면 너무 비싼 것을 한 장으로 그리는 데 쓴다.
	// 픽셀은 0행이 맨 위다. 확대해도 칸이 번지지 않게 가장 가까운 픽셀을 읽는다.
	GLuint CreateImage(int width, int height);
	void UpdateImage(GLuint image, int width, int height, const unsigned char* rgba);
	void DrawImage(GLuint image, float left, float top, float width, float height, float alpha = 1.0f);

	// 이 사각형 밖에는 그려지지 않게 한다. 좌표는 다른 그리기와 같다(화면 중앙 원점, y 위쪽).
	void SetClip(float left, float bottom, float width, float height);
	void ClearClip();

	// ── 모아 그리기 ──────────────────────────────────────────────
	//
	// 사각형과 도형은 바로 그리지 않고 쌓아 두었다가 한 번에 보낸다.
	// 모두 같은 단위 사각형을 쓰고 셰이더도 같으므로, 서로 다른 것은
	// 위치 · 크기 · 색뿐이다. 그 값들만 인스턴스 속성으로 묶어 보내면
	// 수천 개를 glDrawArraysInstanced 한 번으로 그릴 수 있다.
	//
	// 쌓인 것은 다음 경우에 저절로 나간다.
	//   · 다른 종류를 그리려 할 때 (사각형 → 도형, 도형 → 글자)
	//   · 자르기 영역이 바뀔 때
	//   · 프레임이 끝날 때 (Flush를 직접 부른다)
	//
	// 그리는 순서는 그대로 지켜진다. 인스턴스는 넣은 차례대로 래스터화되므로
	// 뒤에서 앞으로 그리는 2.5D 가림이 깨지지 않는다.
	//
	void Flush();

	// 끄면 예전처럼 하나씩 그린다. 전후를 비교해 재려고 남겨 둔 길이다.
	void SetBatching(bool enabled);
	bool IsBatching() const { return m_Batching && m_BatchReady; }

	// 다음 그리기부터 이 꼬리표로 센다. 장면 그래프가 액터마다 바꿔 준다.
	void SetTag(DrawTag tag);

	// 이번 프레임에 꼬리표별로 무엇이 몇 번 있었는가.
	const DrawSample& Sample() const { return m_Sample; }
	void ResetSample() { m_Sample.Clear(); }

	// 이번 프레임에 그린 횟수. 진단용.
	void ResetDrawCount() { m_DrawCount = 0; }
	int DrawCount() const { return m_DrawCount; }

	// 후처리 패스들도 같은 방식으로 셰이더를 불러오므로 공개한다. 실패하면 0.
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	void CreateVertexBufferObjects();
	void GetGLPosition(float x, float y, float *newX, float *newY);
	void DrawRect(float x, float y, float size, float r, float g, float b, float a, float emissive);
	void DrawShape(int shape, float x, float y, float halfWidth, float halfHeight, const Color& color,
	               float rotation, float roundness, float softness, float emissive);

	// ── 모아 그리기 속살 ──
	enum class BatchKind { None, Rect, Shape };

	bool CreateBatchResources();
	void AppendRect(float x, float y, float size, const float rgba[4], float emissive);
	void AppendShape(int shape, float centerX, float centerY, float extentX, float extentY,
	                 const Color& color, float rotation, float roundness, float softness,
	                 float emissive, float halfWidth, float halfHeight);
	void BeginKind(BatchKind kind);
	void FlushRects();
	void FlushShapes();
	void Count(int calls, int instances);

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
	GLint m_LocEmissive = -1;

	// 발광 세기는 대부분 0이다. 같은 값을 매번 다시 보내지 않도록 기억해 둔다.
	float m_LastEmissive = -1.0f;

	// 도형 셰이더
	GLuint m_ShapeShader = 0;
	GLuint m_VBOQuad = 0;
	GLint m_ShapeAttribPosition = -1;
	GLint m_ShapeLocCenter = -1;
	GLint m_ShapeLocExtent = -1;
	GLint m_ShapeLocRotation = -1;
	GLint m_ShapeLocColor = -1;
	GLint m_ShapeLocEmissive = -1;
	GLint m_ShapeLocShape = -1;
	GLint m_ShapeLocHalfSize = -1;
	GLint m_ShapeLocRoundness = -1;
	GLint m_ShapeLocSoftness = -1;

	int m_DrawCount = 0;

	// ── 모아 그리기 상태 ──
	bool m_Batching = true;      // 끄면 예전처럼 하나씩 그린다
	bool m_BatchReady = false;   // 인스턴싱 셰이더와 버퍼를 갖췄는가
	BatchKind m_Kind = BatchKind::None;
	DrawTag m_Tag = DrawTag::Other;
	DrawSample m_Sample;

	// 쌓아 둔 인스턴스. 한 인스턴스가 차지하는 float 개수는 아래 상수와 같다.
	static const int kRectFloats = 9;    // 중심xy, 크기, 미사용, 색rgba, 발광
	static const int kShapeFloats = 15;  // 중심xy+여유xy, 색rgba, 회전·발광·종류·둥글기, 반크기xy+흐림
	std::vector<float> m_RectBatch;
	std::vector<float> m_ShapeBatch;

	// 쌓인 것이 어느 꼬리표의 것인지. 꼬리표가 바뀌면 끊어 보내야
	// 센 값이 섞이지 않는다.
	DrawTag m_BatchTag = DrawTag::Other;

	GLuint m_RectInstShader = 0;
	GLuint m_RectInstVao = 0;
	GLuint m_RectInstVbo = 0;
	size_t m_RectInstCapacity = 0;

	GLuint m_ShapeInstShader = 0;
	GLuint m_ShapeInstVao = 0;
	GLuint m_ShapeInstVbo = 0;
	size_t m_ShapeInstCapacity = 0;

	// 글자
	struct TextSprite
	{
		GLuint texture = 0;   // 0이면 그리기에 실패한 문자열이다. 다시 시도하지 않는다
		int width = 0;
		int height = 0;
	};

	const TextSprite* FindOrCreateText(const std::string& utf8, float pixelSize);

	GLuint m_TextShader = 0;
	GLint m_TextAttribPosition = -1;
	GLint m_TextLocCenter = -1;
	GLint m_TextLocSize = -1;
	GLint m_TextLocColor = -1;
	GLint m_TextLocReveal = -1;
	GLint m_TextLocMode = -1;     // 0 = 글자(알파 마스크), 1 = 이미지(RGBA)
	std::unordered_map<std::string, TextSprite> m_TextCache;
};

