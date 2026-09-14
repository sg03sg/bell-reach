#version 330

// Bloom 축소 단계. 한 단계 내려갈 때마다 해상도가 절반이 된다.
// 13탭 필터라 작은 불꽃이 움직여도 깜빡이거나 네모나게 뭉치지 않는다.
in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_SourceTexel;     // 원본 텍스처의 텍셀 크기 (1/폭, 1/높이)

void main()
{
	vec2 t = u_SourceTexel;

	vec3 a = texture(u_Source, v_UV + t * vec2(-2.0,  2.0)).rgb;
	vec3 b = texture(u_Source, v_UV + t * vec2( 0.0,  2.0)).rgb;
	vec3 c = texture(u_Source, v_UV + t * vec2( 2.0,  2.0)).rgb;
	vec3 d = texture(u_Source, v_UV + t * vec2(-2.0,  0.0)).rgb;
	vec3 e = texture(u_Source, v_UV).rgb;
	vec3 f = texture(u_Source, v_UV + t * vec2( 2.0,  0.0)).rgb;
	vec3 g = texture(u_Source, v_UV + t * vec2(-2.0, -2.0)).rgb;
	vec3 h = texture(u_Source, v_UV + t * vec2( 0.0, -2.0)).rgb;
	vec3 i = texture(u_Source, v_UV + t * vec2( 2.0, -2.0)).rgb;
	vec3 j = texture(u_Source, v_UV + t * vec2(-1.0,  1.0)).rgb;
	vec3 k = texture(u_Source, v_UV + t * vec2( 1.0,  1.0)).rgb;
	vec3 l = texture(u_Source, v_UV + t * vec2(-1.0, -1.0)).rgb;
	vec3 m = texture(u_Source, v_UV + t * vec2( 1.0, -1.0)).rgb;

	// 가중치 합 = 1.0
	vec3 sum = e * 0.125;
	sum += (a + c + g + i) * 0.03125;
	sum += (b + d + f + h) * 0.0625;
	sum += (j + k + l + m) * 0.125;

	FragColor = vec4(sum, 1.0);
}
