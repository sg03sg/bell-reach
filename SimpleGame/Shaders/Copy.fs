#version 330

// 원본을 그대로 옮긴다. 절반 해상도 타깃에 그리면 선형 필터링으로 축소된다.
in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;

void main()
{
	FragColor = vec4(texture(u_Source, v_UV).rgb, 1.0);
}
