#version 330

// 도형을 모아 한 번에 그린다. Shape.vs 와 계산은 같고,
// uniform 이던 값들을 인스턴스 속성으로 받는 것만 다르다.
in vec2 a_Position;              // -0.5 ~ 0.5 단위 사각형. 모든 인스턴스가 함께 쓴다

in vec4 i_CenterExtent;          // xy = 중심(NDC), zw = 그릴 사각형 절반 크기(px)
in vec4 i_Color;
in vec4 i_Params;                // x = 회전(rad), y = 발광, z = 도형 종류, w = 모서리 반경
in vec3 i_HalfSoft;              // xy = 도형 절반 크기(px), z = 경계 흐림 폭

uniform vec2 u_Viewport;

out vec2 v_Local;                // 도형 중심 기준 픽셀 좌표. 회전 전 좌표계다

// 인스턴스마다 고정인 값은 flat 으로 보낸다. 꼭짓점 사이를 보간할 이유가 없다.
flat out vec4  v_Color;
flat out float v_Emissive;
flat out int   v_Shape;
flat out vec2  v_HalfSize;
flat out float v_Roundness;
flat out float v_Softness;

void main()
{
	vec2 local = a_Position * 2.0 * i_CenterExtent.zw;

	float c = cos(i_Params.x);
	float s = sin(i_Params.x);
	vec2 turned = vec2(c * local.x - s * local.y, s * local.x + c * local.y);

	gl_Position = vec4(i_CenterExtent.xy + turned * 2.0 / u_Viewport, 0.0, 1.0);
	v_Local = local;

	v_Color     = i_Color;
	v_Emissive  = i_Params.y;
	v_Shape     = int(i_Params.z + 0.5);
	v_Roundness = i_Params.w;
	v_HalfSize  = i_HalfSoft.xy;
	v_Softness  = i_HalfSoft.z;
}
