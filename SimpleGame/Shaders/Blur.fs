#version 330

// 한 방향 가우시안 블러. 가로 한 번, 세로 한 번을 번갈아 돌린다.
// 9탭 가중치를 선형 필터링 사이 지점에서 읽어 5번 샘플로 줄였다.
in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_Step;        // (1/폭, 0) 또는 (0, 1/높이)에 퍼짐 배율을 곱한 값

void main()
{
	vec3 sum = texture(u_Source, v_UV).rgb * 0.2270270270;
	sum += texture(u_Source, v_UV + u_Step * 1.3846153846).rgb * 0.3162162162;
	sum += texture(u_Source, v_UV - u_Step * 1.3846153846).rgb * 0.3162162162;
	sum += texture(u_Source, v_UV + u_Step * 3.2307692308).rgb * 0.0702702703;
	sum += texture(u_Source, v_UV - u_Step * 3.2307692308).rgb * 0.0702702703;
	FragColor = vec4(sum, 1.0);
}
