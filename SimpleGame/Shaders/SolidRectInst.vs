#version 330

// 사각형을 모아 한 번에 그린다.
//
// a_Position 은 단위 사각형 여섯 꼭짓점이고 모든 인스턴스가 함께 쓴다.
// i_ 로 시작하는 것은 사각형마다 다른 값이라, glVertexAttribDivisor(loc, 1)로
// "꼭짓점마다"가 아니라 "인스턴스마다" 넘어오게 해 둔 속성이다.
in vec3 a_Position;      // 1/창크기 로 미리 나눠 둔 단위 사각형

in vec4 i_Trans;         // xy = 중심(NDC), z = 한 변 길이(px), w = 미사용
in vec4 i_Color;
in float i_Emissive;

flat out vec4 v_Color;
flat out float v_Emissive;

void main()
{
	vec2 p = a_Position.xy * i_Trans.z + i_Trans.xy;
	gl_Position = vec4(p, 0.0, 1.0);

	v_Color = i_Color;
	v_Emissive = i_Emissive;
}
