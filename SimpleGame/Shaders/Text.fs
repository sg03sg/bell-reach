#version 330

in vec2 v_UV;

layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

uniform sampler2D u_Texture;
uniform vec4  u_Color;
uniform float u_Reveal;         // 0 ~ 1. 왼쪽부터 이만큼만 보인다 — 한 글자씩 적히는 효과

// 0 = 글자: R 채널에 글자의 덮임 정도가 들어 있고, 색은 u_Color를 쓴다
// 1 = 이미지: 텍스처의 RGBA를 그대로 쓴다 (미니맵)
uniform int u_Mode;

void main()
{
	if (v_UV.x > u_Reveal)
	{
		discard;
	}

	vec4 sampled = texture(u_Texture, v_UV);
	vec3 rgb = (u_Mode == 1) ? sampled.rgb * u_Color.rgb : u_Color.rgb;
	float alpha = (u_Mode == 1) ? sampled.a * u_Color.a : sampled.r * u_Color.a;
	if (alpha <= 0.002)
	{
		discard;
	}

	FragColor = vec4(rgb, alpha);
	EmissiveColor = vec4(0.0, 0.0, 0.0, alpha);   // 글자와 지도는 빛나지 않는다
}
