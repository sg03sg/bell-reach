#version 330

in vec2 v_UV;

layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

uniform sampler2D u_Texture;    // 한 채널(R)에 글자의 덮임 정도가 들어 있다
uniform vec4  u_Color;
uniform float u_Reveal;         // 0 ~ 1. 왼쪽부터 이만큼만 보인다 — 한 글자씩 적히는 효과

void main()
{
	if (v_UV.x > u_Reveal)
	{
		discard;
	}

	float alpha = u_Color.a * texture(u_Texture, v_UV).r;
	if (alpha <= 0.002)
	{
		discard;
	}

	FragColor = vec4(u_Color.rgb, alpha);
	EmissiveColor = vec4(0.0, 0.0, 0.0, alpha);   // 글자는 빛나지 않는다
}
