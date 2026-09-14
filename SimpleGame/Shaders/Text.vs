#version 330

// 글자 한 줄을 담은 텍스처를 화면에 붙인다.
in vec2 a_Position;             // -0.5 ~ 0.5 단위 사각형

uniform vec2 u_Center;          // 중심 (NDC)
uniform vec2 u_Size;            // 크기 (px)
uniform vec2 u_Viewport;        // 창 크기 (px)

out vec2 v_UV;

void main()
{
	// 글자 비트맵은 0행이 맨 위라, 사각형의 위쪽이 v = 0을 읽게 뒤집는다.
	v_UV = vec2(a_Position.x + 0.5, 0.5 - a_Position.y);
	gl_Position = vec4(u_Center + a_Position * u_Size * 2.0 / u_Viewport, 0.0, 1.0);
}
