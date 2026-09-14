#version 330

// 도형 하나를 담는 사각형. 실제 모양은 프래그먼트 셰이더가 거리 함수로 깎아낸다.
in vec2 a_Position;             // -0.5 ~ 0.5 단위 사각형

uniform vec2  u_Center;         // 도형 중심 (NDC)
uniform vec2  u_Extent;         // 그릴 사각형의 절반 크기(px). 도형보다 가장자리 여유만큼 크다
uniform float u_Rotation;       // 라디안, 반시계 방향
uniform vec2  u_Viewport;       // 창 크기(px)

out vec2 v_Local;               // 도형 중심 기준 픽셀 좌표. 회전하기 전의 좌표계다

void main()
{
	vec2 local = a_Position * 2.0 * u_Extent;

	float c = cos(u_Rotation);
	float s = sin(u_Rotation);
	vec2 turned = vec2(c * local.x - s * local.y, s * local.x + c * local.y);

	gl_Position = vec4(u_Center + turned * 2.0 / u_Viewport, 0.0, 1.0);
	v_Local = local;
}
