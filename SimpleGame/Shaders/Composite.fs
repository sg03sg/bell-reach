#version 330

// 최종 합성.
//   가장자리 흐림 -> Bloom 더하기 -> 노출 -> 톤 매핑 -> 비네트
in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;      // HDR 원본
uniform sampler2D u_Bloom;      // 발광 버퍼에서 번진 빛. 스스로 빛나는 것에서만 나온다
uniform sampler2D u_Blurred;    // 흐리게 만든 장면 전체

uniform float u_Exposure;
uniform float u_BloomStrength;
uniform float u_EdgeBlurStart;
uniform float u_EdgeBlurStrength;
uniform float u_VignetteStart;
uniform float u_VignetteStrength;
uniform vec3  u_VignetteColor;  // 가장자리가 가라앉는 색. 배경 연기와 같다
uniform float u_Aspect;
uniform int   u_ToneMapper;     // 0 = 어깨 곡선, 1 = ACES

// 1.0 아래(knee까지)는 원래 색을 그대로 두고, 그 위만 부드럽게 눌러 담는다.
// 이미 화면값으로 맞춰둔 안개와 벽의 색이 바뀌지 않는다.
vec3 Shoulder(vec3 x)
{
	const float knee = 0.8;
	vec3 over = max(x - knee, 0.0);
	vec3 compressed = knee + (1.0 - knee) * (1.0 - exp(-over / (1.0 - knee)));
	return mix(x, compressed, step(knee, x));
}

// 영화식 곡선. 어두운 곳은 더 가라앉고 중간 밝기는 올라가 대비가 강해진다.
vec3 Aces(vec3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
	// 화면 비율을 보정한 중심 거리. 0 = 한가운데, 1 = 모서리
	vec2 centered = (v_UV - 0.5) * vec2(u_Aspect, 1.0);
	float edge = length(centered) / length(vec2(u_Aspect, 1.0) * 0.5);

	// 가장자리일수록 흐린 장면 쪽으로 섞는다. 시선이 등불이 있는 가운데로 모인다
	vec3 sharp = texture(u_Scene, v_UV).rgb;
	vec3 blurred = texture(u_Blurred, v_UV).rgb;
	float blurAmount = clamp(smoothstep(u_EdgeBlurStart, 1.0, edge) * u_EdgeBlurStrength, 0.0, 1.0);
	vec3 color = mix(sharp, blurred, blurAmount);

	// Bloom은 가장자리 흐림 뒤에 더한다. 빛은 흐려진 가장자리에서도 또렷이 번진다.
	// 톤 매핑 전에 더해야 불꽃 중심이 1.0을 넘어 부드럽게 하얗게 달아오른다.
	color += texture(u_Bloom, v_UV).rgb * u_BloomStrength;
	color *= u_Exposure;
	color = (u_ToneMapper == 1) ? Aces(color) : Shoulder(color);

	// 비네트는 톤 매핑 뒤, 화면값에서 한다. 검정이 아니라 연기 색으로 가라앉는다
	float vignette = clamp(smoothstep(u_VignetteStart, 1.0, edge) * u_VignetteStrength, 0.0, 1.0);
	color = mix(color, u_VignetteColor, vignette);

	FragColor = vec4(color, 1.0);
}
