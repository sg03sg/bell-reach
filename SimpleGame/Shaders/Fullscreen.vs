#version 330

// 화면 전체를 덮는 사각형. 모든 후처리 패스가 이 정점 셰이더를 공유한다.
in vec2 a_Position;
out vec2 v_UV;

void main()
{
	v_UV = a_Position * 0.5 + 0.5;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}
