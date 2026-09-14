#version 330

// Bloom 확대 단계. 작은 단계를 한 단계 큰 쪽으로 펼치며 더한다(가산 블렌딩).
// 여러 크기의 흐림이 겹쳐 쌓이므로, 중심은 진하고 바깥은 멀리까지
// 옅게 이어지는 자연스러운 빛 번짐이 된다. 사각형의 모양이 남지 않는다.
in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_SourceTexel;
uniform float u_Radius;         // 펼치는 폭. 클수록 번짐이 넓고 부드럽다

void main()
{
	vec2 t = u_SourceTexel * u_Radius;

	// 3x3 텐트 필터 (1 2 1 / 2 4 2 / 1 2 1) / 16
	vec3 sum = texture(u_Source, v_UV).rgb * 4.0;

	sum += texture(u_Source, v_UV + vec2(-t.x, 0.0)).rgb * 2.0;
	sum += texture(u_Source, v_UV + vec2( t.x, 0.0)).rgb * 2.0;
	sum += texture(u_Source, v_UV + vec2(0.0, -t.y)).rgb * 2.0;
	sum += texture(u_Source, v_UV + vec2(0.0,  t.y)).rgb * 2.0;

	sum += texture(u_Source, v_UV + vec2(-t.x, -t.y)).rgb;
	sum += texture(u_Source, v_UV + vec2( t.x, -t.y)).rgb;
	sum += texture(u_Source, v_UV + vec2(-t.x,  t.y)).rgb;
	sum += texture(u_Source, v_UV + vec2( t.x,  t.y)).rgb;

	FragColor = vec4(sum / 16.0, 1.0);
}
