#version 330

// 출력이 둘이다.
//   0번: 장면 색
//   1번: 발광 — 스스로 빛나는 것만 값을 가진다. 후처리의 Bloom이 이것만 번지게 한다.
// 후처리가 꺼져 1번 버퍼가 없을 때는 1번 출력이 그냥 버려진다.
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

uniform vec4 u_Color;
uniform float u_Emissive;

void main()
{
	FragColor = vec4(u_Color.r, u_Color.g, u_Color.b, u_Color.a);

	// 알파를 그대로 쓰므로, 발광하지 않는 것(u_Emissive = 0)을 위에 그리면
	// 뒤에 있던 발광을 가린다. 벽 뒤의 불은 번지지 않는다.
	EmissiveColor = vec4(u_Color.rgb * u_Emissive, u_Color.a);
}
