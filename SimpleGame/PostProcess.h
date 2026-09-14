#pragma once

#include "Dependencies/GL_Platform.h"

class Renderer;

//
// 렌더링 결과 기반 후처리.
//
// 장면을 화면에 바로 그리지 않고 부동소수점 버퍼(RGBA16F)에 먼저 그린다.
// 버퍼는 두 장이다.
//
//   장면 버퍼  - 평소대로 그린 색
//   발광 버퍼  - 스스로 빛나는 것(등불, 화톳불, 울린 종)만 기록된 빛의 세기
//
// Bloom은 발광 버퍼만 원본으로 쓴다. 밝기 문턱으로 추리지 않으므로
// 밝게 비춰진 바닥이 번지는 일이 없고, 기름이 줄어 불꽃이 약해져도
// 번짐이 문턱에서 뚝 끊기지 않고 서서히 잦아든다.
//
//   발광 ── 1/2 ─ 1/4 ─ 1/8 ─ ... (축소하며 흐림)
//                                   │
//           1/2 ← 1/4 ← 1/8 ← ... ──┘ (확대하며 겹쳐 쌓음) ── Bloom
//
//   장면 ─┬─ 1/2 축소 → 블러 ──────────────────────────── 가장자리 흐림
//         └──────── 합성(+Bloom) → 톤 매핑 → 비네트 → 화면
//
// 여러 해상도의 흐림을 겹쳐 쌓기 때문에 중심은 진하고 바깥은 멀리까지
// 옅게 이어진다. 작은 사각형 불꽃이라도 네모난 모양이 남지 않고 둥글게 번진다.
//
// 쓰는 쪽은 BeginScene / EndScene 사이에 평소처럼 그리기만 하면 된다.
// 부동소수점 버퍼를 만들 수 없는 환경이면 스스로 꺼지고 화면에 바로 그린다.
//
class PostProcess
{
public:
	// 분위기를 조정하는 값들. 전부 이 한 곳에 모아둔다.
	struct Settings
	{
		float exposure = 1.0f;

		// Bloom
		float bloomStrength = 0.85f;    // 번진 빛을 장면에 더하는 비율
		float bloomRadius = 1.4f;       // 확대 단계에서 펼치는 폭. 클수록 부드럽다
		int   bloomLevels = 6;          // 축소 단계 수. 많을수록 멀리까지 번진다

		// 가장자리 흐림. 중심에서 이 거리(0~1)부터 흐려지기 시작한다.
		float edgeBlurStart = 0.45f;
		float edgeBlurStrength = 1.0f;
		int   edgeBlurIterations = 3;

		// 비네트. 검정이 아니라 배경 연기 색으로 가라앉는다.
		float vignetteStart = 0.35f;
		float vignetteStrength = 0.75f;

		// 0 = 어깨 곡선(기존 색 보존, 밝은 곳만 눌러 담음), 1 = ACES(대비 강함)
		int toneMapper = 0;
	};

	Settings settings;

	bool Initialize(Renderer* renderer, int width, int height);
	void Resize(int width, int height);

	// 이 사이에 그린 것이 후처리를 거친다.
	// 배경색은 비네트가 가라앉는 색으로도 쓰인다.
	void BeginScene(float clearR, float clearG, float clearB);
	void EndScene();

	void SetEnabled(bool enabled) { m_Enabled = enabled; }
	bool IsEnabled() const { return m_Enabled && m_Available; }
	bool IsAvailable() const { return m_Available; }

private:
	static const int kMaxBloomLevels = 8;

	struct Target
	{
		GLuint framebuffer = 0;
		GLuint texture = 0;
		GLuint emissive = 0;    // 장면 버퍼에만 붙는 두 번째 출력
		int width = 0;
		int height = 0;
	};

	struct Pass
	{
		GLuint program = 0;
		GLint position = -1;
	};

	GLuint CreateColorTexture(int width, int height);
	bool CreateTarget(Target& target, int width, int height, bool withEmissive);
	void DestroyTarget(Target& target);
	bool CreateTargets(int width, int height);
	void DestroyTargets();

	bool LoadPass(Renderer* renderer, Pass& pass, const char* fragmentPath);
	void BindTarget(const Target& target);
	void DrawQuad(const Pass& pass);
	void Blur(Target& ping, Target& pong, int iterations);
	void BuildBloom();

	bool m_Available = false;
	bool m_Enabled = true;

	int m_Width = 0;
	int m_Height = 0;
	float m_Clear[3] = { 0.0f, 0.0f, 0.0f };

	Target m_Scene;                         // 전체 해상도. 장면 + 발광 두 장
	Target m_BlurA;                         // 절반 해상도. 가장자리 흐림용
	Target m_BlurB;
	Target m_BloomMips[kMaxBloomLevels];    // 1/2, 1/4, 1/8 ...
	int m_BloomLevelCount = 0;

	Pass m_Copy;
	Pass m_Blur;
	Pass m_BloomDown;
	Pass m_BloomUp;
	Pass m_Composite;

	GLuint m_QuadVao = 0;
	GLuint m_QuadVbo = 0;
};
