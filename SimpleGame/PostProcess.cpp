#include "stdafx.h"
#include "PostProcess.h"

#include "Renderer.h"

#include <algorithm>
#include <iostream>

namespace
{
	const char* const kFullscreenVS = "./Shaders/Fullscreen.vs";

	// 이보다 작아지면 더 축소해도 번짐이 넓어지지 않고 뭉개지기만 한다.
	const int kSmallestBloomSize = 4;
}

bool PostProcess::LoadPass(Renderer* renderer, Pass& pass, const char* fragmentPath)
{
	pass.program = renderer->CompileShaders(kFullscreenVS, fragmentPath);
	if (pass.program == 0)
	{
		return false;
	}

	pass.position = glGetAttribLocation(pass.program, "a_Position");
	return pass.position >= 0;
}

bool PostProcess::Initialize(Renderer* renderer, int width, int height)
{
	m_Available = false;

	if (!LoadPass(renderer, m_Copy, "./Shaders/Copy.fs")
	 || !LoadPass(renderer, m_Blur, "./Shaders/Blur.fs")
	 || !LoadPass(renderer, m_BloomDown, "./Shaders/BloomDown.fs")
	 || !LoadPass(renderer, m_BloomUp, "./Shaders/BloomUp.fs")
	 || !LoadPass(renderer, m_Composite, "./Shaders/Composite.fs"))
	{
		std::cout << "후처리 셰이더를 불러오지 못해 후처리 없이 진행합니다.\n";
		return false;
	}

	// 화면 전체를 덮는 사각형. 렌더러의 VAO와 섞이지 않도록 따로 만든다.
	const float quad[] =
	{
		-1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
		-1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f,
	};

	GLint previousVao = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

	glGenVertexArrays(1, &m_QuadVao);
	glBindVertexArray(m_QuadVao);
	glGenBuffers(1, &m_QuadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	glBindVertexArray(static_cast<GLuint>(previousVao));

	if (!CreateTargets(width, height))
	{
		std::cout << "부동소수점 프레임버퍼를 만들 수 없어 후처리 없이 진행합니다.\n";
		DestroyTargets();
		return false;
	}

	m_Available = true;
	std::cout << "후처리 준비 완료 (" << width << "x" << height
	          << ", Bloom " << m_BloomLevelCount << "단계)"
	          << "  P: 켜기/끄기  T: 톤 매핑 전환\n";
	return true;
}

GLuint PostProcess::CreateColorTexture(int width, int height)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// 16비트 부동소수점. 1.0을 넘는 밝기가 잘리지 않고 남는다 — 이것이 HDR이다.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

	// 선형 필터링. 축소·확대 단계에서 샘플 사이를 보간해 읽는다.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

bool PostProcess::CreateTarget(Target& target, int width, int height, bool withEmissive)
{
	target.width = std::max(1, width);
	target.height = std::max(1, height);
	target.texture = CreateColorTexture(target.width, target.height);

	glGenFramebuffers(1, &target.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);

	if (withEmissive)
	{
		// 두 번째 출력. SolidRect.fs의 location=1이 여기에 기록된다.
		target.emissive = CreateColorTexture(target.width, target.height);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, target.emissive, 0);

		// 출력 버퍼 목록은 프레임버퍼마다 저장되므로 만들 때 한 번만 지정하면 된다.
		const GLenum buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, buffers);
	}

	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return complete;
}

void PostProcess::DestroyTarget(Target& target)
{
	if (target.framebuffer != 0)
	{
		glDeleteFramebuffers(1, &target.framebuffer);
	}
	if (target.texture != 0)
	{
		glDeleteTextures(1, &target.texture);
	}
	if (target.emissive != 0)
	{
		glDeleteTextures(1, &target.emissive);
	}
	target = Target();
}

bool PostProcess::CreateTargets(int width, int height)
{
	m_Width = width;
	m_Height = height;

	if (!CreateTarget(m_Scene, width, height, true)
	 || !CreateTarget(m_BlurA, width / 2, height / 2, false)
	 || !CreateTarget(m_BlurB, width / 2, height / 2, false))
	{
		return false;
	}

	// Bloom 밉 체인. 최대 단계까지 미리 만들어 두어,
	// 실행 중에 settings.bloomLevels를 바꿔도 다시 만들 필요가 없다.
	m_BloomLevelCount = 0;
	int levelWidth = width / 2;
	int levelHeight = height / 2;

	while (m_BloomLevelCount < kMaxBloomLevels
	    && levelWidth >= kSmallestBloomSize && levelHeight >= kSmallestBloomSize)
	{
		if (!CreateTarget(m_BloomMips[m_BloomLevelCount], levelWidth, levelHeight, false))
		{
			return false;
		}
		++m_BloomLevelCount;
		levelWidth /= 2;
		levelHeight /= 2;
	}

	return m_BloomLevelCount > 0;
}

void PostProcess::DestroyTargets()
{
	DestroyTarget(m_Scene);
	DestroyTarget(m_BlurA);
	DestroyTarget(m_BlurB);
	for (int i = 0; i < kMaxBloomLevels; ++i)
	{
		DestroyTarget(m_BloomMips[i]);
	}
	m_BloomLevelCount = 0;
}

void PostProcess::Resize(int width, int height)
{
	if (m_QuadVao == 0 || width <= 0 || height <= 0)
	{
		return;
	}
	if (width == m_Width && height == m_Height)
	{
		return;
	}

	DestroyTargets();
	m_Available = CreateTargets(width, height);
	if (!m_Available)
	{
		DestroyTargets();
		std::cout << "창 크기 변경 후 프레임버퍼를 다시 만들지 못해 후처리를 끕니다.\n";
	}
}

void PostProcess::BindTarget(const Target& target)
{
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glViewport(0, 0, target.width, target.height);
}

void PostProcess::DrawQuad(const Pass& pass)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVbo);
	glEnableVertexAttribArray(pass.position);
	glVertexAttribPointer(pass.position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(pass.position);
}

void PostProcess::Blur(Target& ping, Target& pong, int iterations)
{
	// 결과는 ping에 남는다.
	glUseProgram(m_Blur.program);
	glUniform1i(glGetUniformLocation(m_Blur.program, "u_Source"), 0);
	const GLint step = glGetUniformLocation(m_Blur.program, "u_Step");

	for (int i = 0; i < iterations; ++i)
	{
		BindTarget(pong);
		glBindTexture(GL_TEXTURE_2D, ping.texture);
		glUniform2f(step, 1.0f / ping.width, 0.0f);
		DrawQuad(m_Blur);

		BindTarget(ping);
		glBindTexture(GL_TEXTURE_2D, pong.texture);
		glUniform2f(step, 0.0f, 1.0f / pong.height);
		DrawQuad(m_Blur);
	}
}

void PostProcess::BuildBloom()
{
	const int levels = std::max(1, std::min(settings.bloomLevels, m_BloomLevelCount));

	// ── 축소: 발광 버퍼 → 1/2 → 1/4 → ... ────────────────────
	// 단계마다 해상도가 절반이 되면서 흐려진다.
	glUseProgram(m_BloomDown.program);
	glUniform1i(glGetUniformLocation(m_BloomDown.program, "u_Source"), 0);
	const GLint downTexel = glGetUniformLocation(m_BloomDown.program, "u_SourceTexel");

	GLuint source = m_Scene.emissive;
	int sourceWidth = m_Scene.width;
	int sourceHeight = m_Scene.height;

	for (int i = 0; i < levels; ++i)
	{
		BindTarget(m_BloomMips[i]);
		glBindTexture(GL_TEXTURE_2D, source);
		glUniform2f(downTexel, 1.0f / sourceWidth, 1.0f / sourceHeight);
		DrawQuad(m_BloomDown);

		source = m_BloomMips[i].texture;
		sourceWidth = m_BloomMips[i].width;
		sourceHeight = m_BloomMips[i].height;
	}

	// ── 확대: 가장 작은 단계부터 한 단계씩 펼치며 더한다 ────────
	// 가산 블렌딩이라 큰 단계에 원래 있던 흐림 위에 넓은 흐림이 겹쳐 쌓인다.
	// 이 겹침이 "중심은 진하고 바깥은 멀리까지 옅은" 빛의 모양을 만든다.
	glUseProgram(m_BloomUp.program);
	glUniform1i(glGetUniformLocation(m_BloomUp.program, "u_Source"), 0);
	glUniform1f(glGetUniformLocation(m_BloomUp.program, "u_Radius"), settings.bloomRadius);
	const GLint upTexel = glGetUniformLocation(m_BloomUp.program, "u_SourceTexel");

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	for (int i = levels - 1; i > 0; --i)
	{
		const Target& smaller = m_BloomMips[i];
		BindTarget(m_BloomMips[i - 1]);
		glBindTexture(GL_TEXTURE_2D, smaller.texture);
		glUniform2f(upTexel, 1.0f / smaller.width, 1.0f / smaller.height);
		DrawQuad(m_BloomUp);
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// 결과는 m_BloomMips[0]에 쌓여 있다.
}

void PostProcess::BeginScene(float clearR, float clearG, float clearB)
{
	m_Clear[0] = clearR;
	m_Clear[1] = clearG;
	m_Clear[2] = clearB;

	if (IsEnabled())
	{
		BindTarget(m_Scene);

		// 두 장을 다른 색으로 비운다. 장면은 배경 연기 색, 발광은 0.
		// glClear 한 번이면 발광 버퍼까지 연기 색으로 채워져 화면 전체가 번진다.
		const GLfloat scene[4] = { clearR, clearG, clearB, 1.0f };
		const GLfloat nothing[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glClearBufferfv(GL_COLOR, 0, scene);
		glClearBufferfv(GL_COLOR, 1, nothing);
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (m_Width > 0 && m_Height > 0)
	{
		glViewport(0, 0, m_Width, m_Height);
	}
	glClearColor(clearR, clearG, clearB, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcess::EndScene()
{
	if (!IsEnabled())
	{
		return;   // BeginScene이 이미 화면에 바로 그리게 해 두었다
	}

	// 렌더러가 쓰는 상태를 건드리므로, 끝나면 되돌려 놓는다.
	GLint previousVao = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

	glBindVertexArray(m_QuadVao);
	glDisable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);

	// ── 1. 가장자리 흐림용: 장면을 축소해서 뭉갠다 ─────────────
	BindTarget(m_BlurA);
	glUseProgram(m_Copy.program);
	glUniform1i(glGetUniformLocation(m_Copy.program, "u_Source"), 0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.texture);
	DrawQuad(m_Copy);
	Blur(m_BlurA, m_BlurB, settings.edgeBlurIterations);

	// ── 2. Bloom: 발광 버퍼만 번지게 한다 ───────────────────────
	BuildBloom();

	// ── 3. 합성해서 화면으로 ───────────────────────────────────
	// 패스마다 uniform 조회가 스무 번 남짓이라 캐시하지 않았다.
	// 수천 번 그리는 DrawSolidRect와는 사정이 다르다.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);

	const GLuint program = m_Composite.program;
	glUseProgram(program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BloomMips[0].texture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_BlurA.texture);

	glUniform1i(glGetUniformLocation(program, "u_Scene"), 0);
	glUniform1i(glGetUniformLocation(program, "u_Bloom"), 1);
	glUniform1i(glGetUniformLocation(program, "u_Blurred"), 2);

	glUniform1f(glGetUniformLocation(program, "u_Exposure"), settings.exposure);
	glUniform1f(glGetUniformLocation(program, "u_BloomStrength"), settings.bloomStrength);
	glUniform1f(glGetUniformLocation(program, "u_EdgeBlurStart"), settings.edgeBlurStart);
	glUniform1f(glGetUniformLocation(program, "u_EdgeBlurStrength"), settings.edgeBlurStrength);
	glUniform1f(glGetUniformLocation(program, "u_VignetteStart"), settings.vignetteStart);
	glUniform1f(glGetUniformLocation(program, "u_VignetteStrength"), settings.vignetteStrength);
	glUniform3f(glGetUniformLocation(program, "u_VignetteColor"), m_Clear[0], m_Clear[1], m_Clear[2]);
	glUniform1f(glGetUniformLocation(program, "u_Aspect"),
	            static_cast<float>(m_Width) / static_cast<float>(std::max(1, m_Height)));
	glUniform1i(glGetUniformLocation(program, "u_ToneMapper"), settings.toneMapper);

	DrawQuad(m_Composite);

	// ── 되돌리기 ───────────────────────────────────────────────
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(static_cast<GLuint>(previousVao));
}
