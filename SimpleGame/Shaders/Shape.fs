#version 330

// 부호 거리 함수(SDF)로 도형을 그린다.
// 픽셀마다 "도형 경계까지 몇 픽셀 떨어져 있는가"를 계산해서, 안쪽이면 칠하고
// 경계 근처는 거리만큼 반투명하게 섞는다. 그래서 확대해도 계단이 생기지 않는다.
//
// 출력은 SolidRect.fs와 같이 둘이다. 0번은 장면 색, 1번은 발광(Bloom 원본).
in vec2 v_Local;

layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 EmissiveColor;

uniform vec4  u_Color;
uniform float u_Emissive;
uniform int   u_Shape;          // 0 = 둥근 사각형, 1 = 타원, 2 = 위를 향한 삼각형
uniform vec2  u_HalfSize;       // 도형의 절반 크기(px)
uniform float u_Roundness;      // 사각형 모서리 반경(px)
uniform float u_Softness;       // 경계를 일부러 흐리는 폭(px). 그림자에 쓴다

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
	if (u_Shape == 1)
	{
		dist = Ellipse(v_Local, u_HalfSize);
	}
	else if (u_Shape == 2)
	{
		dist = Triangle(v_Local, u_HalfSize);
	}
	else
	{
		dist = RoundRect(v_Local, u_HalfSize, u_Roundness);
	}

	// 화면에서 한 픽셀이 거리로 얼마인지(fwidth)만큼 경계를 부드럽게 한다.
	// 변수 이름을 distance로 하면 내장 함수를 가려 일부 드라이버가 거부한다.
	float edge = max(fwidth(dist), 0.0001) * 0.75 + u_Softness;
	float coverage = 1.0 - smoothstep(-edge, edge, dist);
	if (coverage <= 0.001)
	{
		discard;   // 여유 공간은 아무것도 쓰지 않는다. 뒤의 발광도 가리지 않는다
	}

	float alpha = u_Color.a * coverage;
	FragColor = vec4(u_Color.rgb, alpha);
	EmissiveColor = vec4(u_Color.rgb * u_Emissive, alpha);
}
