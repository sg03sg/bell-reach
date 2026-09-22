#version 330

// SolidRect.fs 와 하는 일이 같다. 색과 발광을 uniform이 아니라
// 정점 셰이더에서 받는 것만 다르다.
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

flat in vec4 v_Color;
flat in float v_Emissive;

void main()
{
	FragColor = v_Color;

	// 알파를 그대로 쓰므로, 발광하지 않는 것을 위에 그리면 뒤의 발광을 가린다.
	EmissiveColor = vec4(v_Color.rgb * v_Emissive, v_Color.a);
}
