#version 330

// Shape.fs 와 거리 함수가 같다. 도형 정보를 uniform 이 아니라
// 정점 셰이더에서 받는 것만 다르다.
in vec2 v_Local;

flat in vec4  v_Color;
flat in float v_Emissive;
flat in int   v_Shape;
flat in vec2  v_HalfSize;
flat in float v_Roundness;
flat in float v_Softness;

layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

float RoundRect(vec2 p, vec2 halfSize, float radius)
{
	radius = min(radius, min(halfSize.x, halfSize.y));
	vec2 q = abs(p) - halfSize + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float Ellipse(vec2 p, vec2 radii)
{
	float k0 = length(p / radii);
	float k1 = length(p / (radii * radii));
	if (k1 < 0.0001)
	{
		return -min(radii.x, radii.y);   // 정중앙
	}
	return k0 * (k0 - 1.0) / k1;
}

// 꼭짓점이 (0, h), 밑변이 y = -h 인 이등변 삼각형.
float Triangle(vec2 p, vec2 halfSize)
{
	vec2 q = vec2(halfSize.x, -2.0 * halfSize.y);
	p.y -= halfSize.y;
	p.x = abs(p.x);

	vec2 a = p - q * clamp(dot(p, q) / dot(q, q), 0.0, 1.0);
	vec2 b = p - q * vec2(clamp(p.x / q.x, 0.0, 1.0), 1.0);
	float s = -sign(q.y);
	vec2 d = min(vec2(dot(a, a), s * (p.x * q.y - p.y * q.x)),
	             vec2(dot(b, b), s * (p.y - q.y)));
	return -sqrt(d.x) * sign(d.y);
}

void main()
{
	float dist;
	if (v_Shape == 1)
	{
		dist = Ellipse(v_Local, v_HalfSize);
	}
	else if (v_Shape == 2)
	{
		dist = Triangle(v_Local, v_HalfSize);
	}
	else
	{
		dist = RoundRect(v_Local, v_HalfSize, v_Roundness);
	}

	// 화면에서 한 픽셀이 거리로 얼마인지(fwidth)만큼 경계를 부드럽게 한다.
	float edge = max(fwidth(dist), 0.0001) * 0.75 + v_Softness;
	float coverage = 1.0 - smoothstep(-edge, edge, dist);
	if (coverage <= 0.001)
	{
		discard;   // 여유 공간은 아무것도 쓰지 않는다. 뒤의 발광도 가리지 않는다
	}

	float alpha = v_Color.a * coverage;
	FragColor = vec4(v_Color.rgb, alpha);
	EmissiveColor = vec4(v_Color.rgb * v_Emissive, alpha);
}
