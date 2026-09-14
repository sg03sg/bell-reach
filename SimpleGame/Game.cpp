#include "stdafx.h"
#include "Game.h"

#include "Input.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace
{
	const int   kTileSize   = 32;

	// 밝기 격자는 타일보다 촘촘하게 잡는다. 조명이 타일 단위로 각지지 않는다.
	const float kLightCell  = 16.0f;

	// ── 안개 ────────────────────────────────────────────────────
	// 세계는 검은 연기에 덮여 있다. 완전한 암흑이 아니라서 지형의 윤곽은
	// 겨우 알아볼 수 있지만, 그 안에 무엇이 있는지는 불을 가져가야 보인다.
	const float kFogBrightness = 0.22f;

	// 오브젝트와 사람이 모습을 드러내기 시작하는 밝기.
	// 안개 밝기보다 확실히 높아야 "불로 밝혀야 보인다"가 성립한다.
	const float kRevealAt = 0.30f;

	// ── 등불 ────────────────────────────────────────────────────
	const float kLanternRadius = 155.0f;
	const float kOilSeconds = 150.0f;   // 가득 찬 기름으로 버티는 시간

	// ── 화톳불 ──────────────────────────────────────────────────
	const float kFireRadius = 130.0f;

	// ── 굳은 사람 ───────────────────────────────────────────────
	// 어둠에 잡힌 사람은 숨어 있지 않다. 연기 너머로도 형체가 보인다.
	// 다만 죽은 돌처럼 서 있을 뿐이라, 불을 들이대고 버텨야 생기가 돈다.
	const float kReviveSeconds = 2.0f;   // 불을 비춘 채 버텨야 하는 시간
	const float kChillSeconds = 4.0f;    // 등불이 떠나면 다시 식는 시간

	// ── 종탑 ────────────────────────────────────────────────────
	const float kRingSeconds = 4.5f;     // 종이 제대로 울리기까지
	const float kTowerReach = 46.0f;     // 플레이어가 밧줄에 닿는 거리
	const float kCompanionReach = 95.0f; // 동행자가 함께 당길 수 있는 거리
	const float kWakeReach = 36.0f;      // 굳은 사람에게 말을 거는 거리

	// 울린 종탑이 밝히는 범위와 밝기. 구역 한 변의 절반쯤이라
	// 구역 하나를 되찾으면 그 일대가 안전해진다.
	const float kBeaconBrightness = 0.62f;

	// ── 궤적 ────────────────────────────────────────────────────
	const float kFollowLag = 46.0f;      // 동행자가 뒤처지는 거리
	const float kPathKeep = 420.0f;      // 궤적을 얼마나 오래 들고 있을지
	const float kPathStep = 3.0f;        // 이만큼 움직여야 점을 하나 찍는다

	// ── 색 ──────────────────────────────────────────────────────
	// 채도를 낮게 잡아, 불의 호박색만 따뜻하게 남긴다.
	const float kFloorColor[3] = { 0.26f, 0.28f, 0.32f };
	const float kWallColor[3]  = { 0.09f, 0.10f, 0.14f };
	const float kFlame[3]      = { 0.98f, 0.72f, 0.36f };
	const float kRubble[3]     = { 0.42f, 0.38f, 0.31f };
	const float kSupply[3]     = { 0.45f, 0.62f, 0.52f };
	const float kStone[3]      = { 0.34f, 0.36f, 0.40f };
	const float kCrow[3]       = { 0.02f, 0.02f, 0.03f };
	const float kLiving[3]     = { 0.92f, 0.74f, 0.58f };   // 깨어난 사람의 살색
	const float kFrozen[3]     = { 0.10f, 0.10f, 0.13f };   // 굳어버린 사람. 연기보다 겨우 밝다

	// ── 발광 ────────────────────────────────────────────────────
	// 스스로 빛나는 것은 DrawEmissiveRect로 그린다. 불꽃 자체는 불꽃 색 그대로
	// 그리고, 이 세기만큼의 빛을 발광 버퍼에 따로 남긴다. 후처리 Bloom이
	// 그 빛을 번지게 하므로, 빛은 사각형이 아니라 번짐으로 보인다.
	//
	// 번짐의 크기는 "세기 x 도형 면적"에 비례한다. 작은 도형일수록 세기를 높게.
	const float kGlowLantern = 8.0f;    // 등불 유리창. 심지는 1.8배. 기름이 줄면 함께 약해진다
	const float kGlowFire    = 5.0f;    // 화톳불 불꽃 가운데 겹. 바깥 겹은 절반, 심지는 1.8배
	const float kGlowBell    = 8.0f;    // 울린 종. 멀리서 보이는 표식
	const float kGlowSpark   = 9.0f;    // 말을 걸 수 있게 된 사람 머리 위 불씨
	const float kGlowGauge   = 5.0f;    // 종을 당길 때 차오르는 눈금

	float Mix(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	int FloorToInt(float value)
	{
		return static_cast<int>(std::floor(value));
	}

	float Distance(float ax, float ay, float bx, float by)
	{
		const float dx = ax - bx;
		const float dy = ay - by;
		return std::sqrt(dx * dx + dy * dy);
	}

	float TileToWorld(int tile)
	{
		return tile * kTileSize + kTileSize * 0.5f;
	}

	float BeaconRadius()
	{
		return Regions::RegionTileSpan() * kTileSize * 0.55f;
	}
}

namespace
{
	// ══ 오브젝트 도안 ════════════════════════════════════════════
	// 오브젝트는 전부 발밑이 기준점이고 위로 자란다(렌더러 좌표는 y가 위).
	// 발밑 y로 정렬해 그리므로 화면 아래쪽에 선 것이 앞에 온다 — 2.5D의 기본이다.

	const float kCloak[3]     = { 0.24f, 0.21f, 0.18f };   // 플레이어 망토
	const float kHood[3]      = { 0.15f, 0.14f, 0.13f };
	const float kTunic[3]     = { 0.38f, 0.31f, 0.24f };   // 깨어난 사람의 옷
	const float kHair[3]      = { 0.19f, 0.14f, 0.10f };
	const float kMetal[3]     = { 0.32f, 0.30f, 0.27f };
	const float kWood[3]      = { 0.34f, 0.23f, 0.14f };
	const float kWoodDark[3]  = { 0.20f, 0.14f, 0.09f };
	const float kEmber[3]     = { 1.00f, 0.42f, 0.14f };
	const float kFlameCore[3] = { 1.00f, 0.92f, 0.70f };
	const float kStoneDark[3] = { 0.21f, 0.22f, 0.25f };
	const float kRoof[3]      = { 0.17f, 0.15f, 0.17f };
	const float kBronze[3]    = { 0.58f, 0.44f, 0.22f };
	const float kRope[3]      = { 0.45f, 0.38f, 0.26f };

	const int kDrawTower     = 0;
	const int kDrawSleeper   = 1;
	const int kDrawProp      = 2;
	const int kDrawCompanion = 3;
	const int kDrawFire      = 4;
	const int kDrawPlayer    = 5;

	Color Tint(const float c[3], float shade = 1.0f, float alpha = 1.0f)
	{
		Color color = { c[0] * shade, c[1] * shade, c[2] * shade, alpha };
		return color;
	}

	Color Blend(const float a[3], const float b[3], float t, float shade = 1.0f)
	{
		Color color = { Mix(a[0], b[0], t) * shade,
		                Mix(a[1], b[1], t) * shade,
		                Mix(a[2], b[2], t) * shade,
		                1.0f };
		return color;
	}

	// 발밑 그림자. 경계를 흐려 바닥에 붙어 있는 느낌을 준다.
	void DrawShadow(Renderer* r, float x, float y, float radiusX, float alpha = 0.38f)
	{
		const Color shadow = { 0.0f, 0.0f, 0.0f, alpha };
		r->DrawEllipse(x, y, radiusX, radiusX * 0.36f, shadow, 0.0f, 0.0f, 2.5f);
	}

	struct FigureLook
	{
		Color body;
		Color skin;
		Color hair;        // 두건이면 두건 색, 아니면 머리카락 색
		bool  hooded;
		float posture;     // 0 = 웅크림, 1 = 바로 섬
		float facing;      // -1 왼쪽, 1 오른쪽
		float bob;         // 걸음에 따른 들썩임(px)
	};

	// 사람 한 명. 발밑 (x, y)에서 위로 약 32px. 앞으로 뻗은 손의 위치를 돌려준다.
	void DrawFigure(Renderer* r, float x, float y, const FigureLook& look,
	                float* outHandX = NULL, float* outHandY = NULL)
	{
		const float height = Mix(0.70f, 1.0f, look.posture);
		const float hunch = 1.0f - look.posture;
		const float f = look.facing;
		const float lift = look.bob;

		DrawShadow(r, x, y, 10.0f);

		// 망토 자락 — 아래로 퍼진다
		r->DrawTriangle(x, y + 9.0f * height + lift * 0.3f, 19.0f, 18.0f * height, look.body);

		// 몸통 — 웅크리면 앞으로 기운다
		const float torsoY = y + 16.0f * height + lift;
		r->DrawRoundRect(x + f * hunch * 2.0f, torsoY, 12.0f, 14.0f * height, 5.0f, look.body,
		                 -f * hunch * 0.45f);

		// 팔 — 웅크리면 몸 쪽으로 끌어안는다
		const float shoulderX = x + f * (3.0f + hunch * 2.0f);
		const float shoulderY = torsoY + 4.0f * height;
		const float handX = x + f * (8.0f - hunch * 3.0f);
		const float handY = y + (11.0f + hunch * 2.0f) * height + lift;
		r->DrawSegment(shoulderX, shoulderY, handX, handY, 3.6f, look.body);

		// 머리 — 웅크리면 앞으로 떨군다
		const float headX = x + f * hunch * 5.5f;
		const float headY = y + (26.0f - hunch * 3.0f) * height + lift;

		if (look.hooded)
		{
			r->DrawEllipse(headX - f * 0.6f, headY + 0.6f, 6.4f, 6.6f, look.hair);
			r->DrawTriangle(headX - f * 3.8f, headY + 4.2f, 6.0f, 6.5f, look.hair, f * 0.9f);   // 두건 꼬리
			r->DrawEllipse(headX + f * 1.9f, headY - 0.6f, 3.2f, 3.8f, look.skin);               // 얼굴
		}
		else
		{
			r->DrawCircle(headX, headY, 5.2f, look.skin);
			r->DrawEllipse(headX - f * 1.0f, headY + 2.4f, 5.4f, 3.2f, look.hair, -f * 0.2f);
		}

		if (outHandX != NULL) { *outHandX = handX; }
		if (outHandY != NULL) { *outHandY = handY; }
	}

	// 손에 매달린 등불. 손을 축으로 흔들리는 진자다. glow = 0이면 꺼진 등불이다.
	void DrawLantern(Renderer* r, float handX, float handY, float swing, float glow)
	{
		const float hang = 6.0f;
		const float x = handX + std::sin(swing) * hang;
		const float y = handY - std::cos(swing) * hang;

		r->DrawSegment(handX, handY, x, y + 4.5f, 1.2f, Tint(kMetal));
		r->DrawTriangle(x, y + 5.5f, 7.5f, 3.5f, Tint(kMetal), swing);          // 뚜껑
		r->DrawRoundRect(x, y, 7.5f, 9.0f, 2.0f, Tint(kMetal), swing);          // 틀
		r->DrawRoundRect(x, y - 0.2f, 4.6f, 6.4f, 1.5f,                          // 유리 속 불빛
		                 Blend(kWoodDark, kFlame, glow), swing, kGlowLantern * glow);

		if (glow > 0.0f)
		{
			r->DrawEllipse(x, y - 0.8f, 1.3f, 2.1f, Tint(kFlameCore), swing,     // 심지
			               kGlowLantern * 1.8f * glow);
		}
	}

	// 화톳불. 돌을 둘러 장작을 엇갈려 쌓고, 불꽃 세 겹이 다른 박자로 일렁인다.
	void DrawBonfire(Renderer* r, float x, float y, float time, float seed)
	{
		const Color scorch = { 0.0f, 0.0f, 0.0f, 0.45f };
		r->DrawEllipse(x, y, 15.0f, 5.5f, scorch, 0.0f, 0.0f, 2.5f);

		for (int i = 0; i < 7; ++i)
		{
			const float angle = i * 6.2832f / 7.0f + seed;
			const float shade = 0.55f + 0.2f * std::sin(angle * 3.0f + seed);
			r->DrawEllipse(x + std::cos(angle) * 12.5f, y + std::sin(angle) * 4.5f + 1.2f,
			               3.2f, 2.3f, Tint(kStone, shade));
		}

		r->DrawSegment(x - 9.0f, y - 1.0f, x + 9.0f, y + 5.0f, 4.2f, Tint(kWood));
		r->DrawSegment(x - 9.0f, y + 5.0f, x + 9.0f, y - 1.0f, 4.2f, Tint(kWoodDark));

		const float flickerA = std::sin(time * 9.0f + seed) * 0.5f + 0.5f;
		const float flickerB = std::sin(time * 13.0f + seed * 1.7f) * 0.5f + 0.5f;
		const float sway = std::sin(time * 3.1f + seed) * 1.6f;

		const float outer = 17.0f + flickerA * 6.0f;
		r->DrawTriangle(x + sway, y + 3.0f + outer * 0.5f, 13.0f, outer,
		                Tint(kEmber), -sway * 0.05f, kGlowFire * 0.5f);

		const float inner = 12.0f + flickerB * 4.0f;
		r->DrawTriangle(x - sway * 0.4f, y + 3.0f + inner * 0.5f, 8.5f, inner,
		                Tint(kFlame), sway * 0.04f, kGlowFire);

		r->DrawEllipse(x, y + 6.0f, 2.8f, 3.8f + flickerA, Tint(kFlameCore), 0.0f, kGlowFire * 1.8f);

		// 튀어 오르며 사그라드는 불티
		const float rise = std::fmod(time * 22.0f + seed * 40.0f, 26.0f);
		const float fade = 1.0f - rise / 26.0f;
		r->DrawCircle(x + std::sin(time * 5.0f + seed) * 3.0f, y + 16.0f + rise, 1.1f,
		              Tint(kEmber, 1.0f, fade), kGlowFire * 0.8f * fade);
	}

	// 까마귀 한 마리. 가만히 앉아 있다가 가끔 고개를 돌린다.
	void DrawCrow(Renderer* r, float x, float y, float facing, float time, float seed)
	{
		const float bob = std::sin(time * 2.3f + seed) * 0.6f;
		const float look = (std::sin(time * 0.7f + seed * 3.0f) > 0.6f) ? -facing : facing;
		const Color black = Tint(kCrow);

		r->DrawTriangle(x - facing * 5.0f, y + 2.5f + bob, 4.0f, 5.0f, black, facing * 1.9f);   // 꼬리
		r->DrawEllipse(x, y + 3.0f + bob, 4.6f, 3.0f, black, facing * 0.25f);                  // 몸
		r->DrawCircle(x + look * 3.4f, y + 5.4f + bob, 2.2f, black);                           // 머리
		r->DrawTriangle(x + look * 6.0f, y + 5.2f + bob, 2.2f, 3.0f, black, -look * 1.5708f);   // 부리
	}

	unsigned int PropHash(int x, int y)
	{
		unsigned int h = (static_cast<unsigned int>(x) * 73856093u) ^ (static_cast<unsigned int>(y) * 19349663u);
		h ^= h >> 13;
		h *= 0x5BD1E995u;
		h ^= h >> 15;
		return h;
	}

	// 오브젝트. 같은 자리의 것은 항상 같은 모양이 되도록 좌표로 흔든다.
	void DrawProp(Renderer* r, float x, float y, const World::Prop& prop, float shade)
	{
		const unsigned int hash = PropHash(prop.tileX, prop.tileY);
		const float tilt = (static_cast<int>(hash % 11u) - 5) * 0.08f;
		const float flip = (hash & 1u) ? 1.0f : -1.0f;

		DrawShadow(r, x, y, 12.0f, 0.32f * shade);

		if (prop.kind == 1)
		{
			// 보급품 — 나무 상자와 천 자루
			const float crateX = x - flip * 3.0f;
			r->DrawRoundRect(crateX, y + 6.5f, 15.0f, 12.0f, 1.5f, Tint(kWood, shade), tilt * 0.3f);
			r->DrawRoundRect(crateX, y + 9.5f, 15.0f, 2.2f, 0.8f, Tint(kWoodDark, shade), tilt * 0.3f);
			r->DrawSegment(crateX - 5.5f, y + 1.8f, crateX + 5.5f, y + 11.5f, 1.6f, Tint(kWoodDark, shade));

			const float sackX = x + flip * 7.5f;
			r->DrawEllipse(sackX, y + 5.0f, 5.2f, 5.8f, Tint(kSupply, shade), flip * 0.15f);
			r->DrawCircle(sackX, y + 10.8f, 1.6f, Tint(kRope, shade));
		}
		else
		{
			// 잔해 — 무너진 돌과 부러진 판자
			r->DrawEllipse(x - flip * 5.0f, y + 3.5f, 6.0f, 4.4f, Tint(kRubble, shade * 0.85f), tilt);
			r->DrawEllipse(x + flip * 4.5f, y + 2.8f, 4.8f, 3.5f, Tint(kStone, shade * 0.9f), -tilt);
			r->DrawSegment(x - 8.0f, y + 4.0f + tilt * 10.0f, x + 8.0f, y + 6.0f - tilt * 10.0f,
			               3.0f, Tint(kWoodDark, shade));
			r->DrawEllipse(x + flip * 0.5f, y + 7.5f, 3.8f, 2.9f, Tint(kRubble, shade), tilt * 2.0f);
		}
	}
}

void Game::Initialize(int windowWidth, int windowHeight)
{
	m_WindowWidth = windowWidth;
	m_WindowHeight = windowHeight;

	m_Light.Configure(kLightCell);

	// 첫 종탑에서 몇 걸음 떨어진 곳에서 시작한다.
	// 종탑이 있는 청크의 십자 통로 위라 어느 씨앗에서도 바닥이 보장되고,
	// 시작하자마자 연기 너머로 종탑이 보여 갈 곳이 생긴다.
	const Regions::Region first = m_Regions.At(0, 0);
	m_PlayerX = TileToWorld(first.towerTileX - 7);
	m_PlayerY = TileToWorld(first.towerTileY);

	m_CameraX = m_PlayerX;
	m_CameraY = m_PlayerY;

	m_PlayerPath.push_back({ m_PlayerX, m_PlayerY, 0.0f });
}

bool Game::IsWallAtWorld(float worldX, float worldY) const
{
	return m_World.IsWall(FloorToInt(worldX / kTileSize),
	                      FloorToInt(worldY / kTileSize));
}

bool Game::IsPlayerBlocked(float worldX, float worldY) const
{
	// 플레이어 사각형의 네 모서리를 검사한다.
	// 사각형끼리의 판정이라 렌더러가 그리는 것과 같은 수학이다.
	const float h = m_PlayerHalfSize;
	return IsWallAtWorld(worldX - h, worldY - h)
	    || IsWallAtWorld(worldX + h, worldY - h)
	    || IsWallAtWorld(worldX - h, worldY + h)
	    || IsWallAtWorld(worldX + h, worldY + h);
}

void Game::MovePlayer(float deltaSeconds, const Input& input)
{
	float moveX = 0.0f;
	float moveY = 0.0f;

	if (input.MoveLeft())  { moveX -= 1.0f; }
	if (input.MoveRight()) { moveX += 1.0f; }
	if (input.MoveUp())    { moveY -= 1.0f; }
	if (input.MoveDown())  { moveY += 1.0f; }

	// 대각선이 더 빠르지 않도록 정규화한다.
	const float length = std::sqrt(moveX * moveX + moveY * moveY);
	m_PlayerMoving = length > 0.0f;
	if (moveX != 0.0f)
	{
		m_FacingX = (moveX > 0.0f) ? 1.0f : -1.0f;
	}
	if (length <= 0.0f)
	{
		return;
	}

	const float step = m_PlayerSpeed * deltaSeconds;
	const float dx = moveX / length * step;
	const float dy = moveY / length * step;

	// 축을 나눠서 이동해야 벽에 부딪혔을 때 미끄러진다.
	if (!IsPlayerBlocked(m_PlayerX + dx, m_PlayerY)) { m_PlayerX += dx; }
	if (!IsPlayerBlocked(m_PlayerX, m_PlayerY + dy)) { m_PlayerY += dy; }
}

void Game::RecordPath()
{
	const PathPoint& last = m_PlayerPath.back();
	const float moved = Distance(m_PlayerX, m_PlayerY, last.x, last.y);

	if (moved < kPathStep)
	{
		return;
	}

	m_PathLength += moved;
	m_PlayerPath.push_back({ m_PlayerX, m_PlayerY, m_PathLength });

	// 필요한 만큼만 들고 있는다. 무한 맵을 걸어도 궤적이 무한히 늘지 않는다.
	while (m_PlayerPath.size() > 2
	    && m_PathLength - m_PlayerPath.front().travelled > kPathKeep)
	{
		m_PlayerPath.pop_front();
	}
}

bool Game::CompanionTarget(float& outX, float& outY) const
{
	if (m_PlayerPath.size() < 2)
	{
		return false;
	}

	// 궤적 위에서 kFollowLag 만큼 뒤처진 지점.
	// 길찾기가 아니라 "플레이어가 조금 전에 서 있던 자리"라서
	// 벽에 낄 수가 없다.
	const float want = m_PathLength - kFollowLag;

	if (want <= m_PlayerPath.front().travelled)
	{
		outX = m_PlayerPath.front().x;
		outY = m_PlayerPath.front().y;
		return true;
	}

	for (size_t i = m_PlayerPath.size() - 1; i > 0; --i)
	{
		const PathPoint& before = m_PlayerPath[i - 1];
		const PathPoint& after = m_PlayerPath[i];

		if (before.travelled <= want && want <= after.travelled)
		{
			const float span = after.travelled - before.travelled;
			const float t = (span > 0.0f) ? (want - before.travelled) / span : 0.0f;
			outX = Mix(before.x, after.x, t);
			outY = Mix(before.y, after.y, t);
			return true;
		}
	}

	return false;
}

void Game::RefreshNearbyRegions()
{
	// 화면보다 넉넉하게 잡아, 화면 밖 종탑의 불빛도 가장자리로 새어 들어온다.
	const float margin = BeaconRadius();
	const int tileMinX = FloorToInt((m_CameraX - m_WindowWidth * 0.5f - margin) / kTileSize);
	const int tileMaxX = FloorToInt((m_CameraX + m_WindowWidth * 0.5f + margin) / kTileSize);
	const int tileMinY = FloorToInt((m_CameraY - m_WindowHeight * 0.5f - margin) / kTileSize);
	const int tileMaxY = FloorToInt((m_CameraY + m_WindowHeight * 0.5f + margin) / kTileSize);

	m_NearbyRegions.clear();
	m_Regions.CollectOverlapping(tileMinX, tileMinY, tileMaxX, tileMaxY, m_NearbyRegions);

	// 울린 종탑만 따로 모아둔다. 밝기를 물어볼 때마다 전부 훑지 않기 위해서다.
	m_Beacons.clear();
	for (const Regions::Region& region : m_NearbyRegions)
	{
		if (region.rung)
		{
			m_Beacons.push_back({ TileToWorld(region.towerTileX),
			                      TileToWorld(region.towerTileY) });
		}
	}
}

float Game::BeaconBrightness(float worldX, float worldY) const
{
	// 울린 종탑 주변은 영구히 밝다. 사각형 구역이 아니라 원형으로 퍼지게 해
	// "종소리가 닿는 곳까지"가 화면에 그대로 보이게 한다.
	const float radius = BeaconRadius();
	const float inner = radius * 0.7f;

	float best = 0.0f;
	for (const Beacon& beacon : m_Beacons)
	{
		const float distance = Distance(worldX, worldY, beacon.x, beacon.y);
		if (distance >= radius)
		{
			continue;
		}

		float falloff = 1.0f;
		if (distance > inner)
		{
			falloff = 1.0f - (distance - inner) / (radius - inner);
			falloff = falloff * falloff * (3.0f - 2.0f * falloff);
		}

		best = std::max(best, kBeaconBrightness * falloff);
	}

	return best;
}

float Game::TotalBrightness(float worldX, float worldY) const
{
	return std::max(m_Light.BrightnessAt(worldX, worldY),
	                BeaconBrightness(worldX, worldY));
}

void Game::UpdateSleepers(float deltaSeconds)
{
	// 굳은 사람은 불을 비추고 있는 동안 조금씩 생기를 되찾는다.
	// 등불은 내가 선 자리만 밝히므로, 자리를 뜨면 다시 식기 시작한다.
	// 그래서 데려가려면 말을 걸어 따라오게 하는 수밖에 없다 —
	// "혼자서는 안 된다"가 이 규칙에서 한 번 더 나온다.
	for (Regions::Region& region : m_NearbyRegions)
	{
		if (region.woken)
		{
			continue;
		}

		const float sleeperX = TileToWorld(region.sleeperTileX);
		const float sleeperY = TileToWorld(region.sleeperTileY);
		const float brightness = TotalBrightness(sleeperX, sleeperY);

		float warmth = region.warmth;
		if (brightness >= kRevealAt)
		{
			warmth += deltaSeconds / kReviveSeconds;
		}
		else
		{
			warmth -= deltaSeconds / kChillSeconds;
		}

		warmth = std::max(0.0f, std::min(1.0f, warmth));

		if (warmth != region.warmth)
		{
			region.warmth = warmth;
			m_Regions.SetWarmth(region.regionX, region.regionY, warmth);
		}
	}
}

bool Game::TryWake()
{
	if (m_Companion.IsAwake())
	{
		return false;   // 한 번에 한 사람만 데리고 다닌다
	}

	for (const Regions::Region& region : m_NearbyRegions)
	{
		if (region.woken)
		{
			continue;
		}

		// 생기가 다 돌아와야 말을 알아듣는다.
		// 불을 들이대고 버티는 2초가 이 게임에서 사람을 되찾는 시간이다.
		if (region.warmth < 1.0f)
		{
			continue;
		}

		const float sleeperX = TileToWorld(region.sleeperTileX);
		const float sleeperY = TileToWorld(region.sleeperTileY);

		if (Distance(m_PlayerX, m_PlayerY, sleeperX, sleeperY) > kWakeReach)
		{
			continue;
		}

		m_Companion.Wake(sleeperX, sleeperY, region.regionX, region.regionY);
		m_Regions.MarkWoken(region.regionX, region.regionY);
		return true;
	}

	return false;
}

void Game::UpdateRinging(float deltaSeconds, bool holding)
{
	// 큰 종은 혼자 울릴 수 없다.
	// 플레이어와 동행자가 함께 종탑에 붙어 있어야 게이지가 오른다.
	bool canRing = false;
	int ringingRegionX = 0;
	int ringingRegionY = 0;

	if (m_Companion.IsAwake() && !m_Companion.IsFrozenAgain())
	{
		for (const Regions::Region& region : m_NearbyRegions)
		{
			if (region.rung)
			{
				continue;
			}

			const float towerX = TileToWorld(region.towerTileX);
			const float towerY = TileToWorld(region.towerTileY);

			if (Distance(m_PlayerX, m_PlayerY, towerX, towerY) <= kTowerReach
			 && Distance(m_Companion.X(), m_Companion.Y(), towerX, towerY) <= kCompanionReach)
			{
				canRing = true;
				ringingRegionX = region.regionX;
				ringingRegionY = region.regionY;
				break;
			}
		}
	}

	if (!canRing || !holding)
	{
		// 손을 놓으면 종이 다시 잦아든다. 한 번에 끝까지 당겨야 한다.
		m_RingProgress = std::max(0.0f, m_RingProgress - deltaSeconds / (kRingSeconds * 0.6f));
		return;
	}

	m_RingProgress += deltaSeconds / kRingSeconds;

	if (m_RingProgress >= 1.0f)
	{
		m_RingProgress = 0.0f;
		m_Regions.MarkRung(ringingRegionX, ringingRegionY);

		// 동행자는 종탑에 남아 종을 울린다. 그래서 이 구역이 계속 밝다.
		m_Companion.Dismiss();

		RefreshNearbyRegions();   // 방금 켜진 종탑을 바로 반영한다
	}
}

void Game::HandleInteraction(float deltaSeconds, const Input& input)
{
	const bool interact = input.Interact();
	const bool interactPressed = interact && !m_PrevInteract;

	if (interactPressed && TryWake())
	{
		// 방금 깨운 사람이 같은 프레임에 굳은 형상으로도 그려지지 않도록 갱신한다.
		RefreshNearbyRegions();
	}

	UpdateRinging(deltaSeconds, interact);

	const bool placeFire = input.PlaceFire();
	if (placeFire && !m_PrevPlaceFire && m_FiresLeft > 0)
	{
		m_Fires.push_back({ m_PlayerX, m_PlayerY, kFireRadius });
		--m_FiresLeft;
	}

	m_PrevInteract = interact;
	m_PrevPlaceFire = placeFire;
}

void Game::Update(float deltaSeconds, const Input& input)
{
	m_Time += deltaSeconds;

	MovePlayer(deltaSeconds, input);
	RecordPath();

	// 카메라는 살짝 늦게 따라온다. 방향을 꺾을 때 화면이 덜 튄다.
	const float follow = std::min(1.0f, deltaSeconds * 8.0f);
	m_CameraX += (m_PlayerX - m_CameraX) * follow;
	m_CameraY += (m_PlayerY - m_CameraY) * follow;

	RefreshNearbyRegions();

	// 기름은 계속 준다. 이미 되찾은 구역 안에서는 다시 채운다 —
	// 밝혀 놓은 땅이 보급선이 되는 셈이다.
	if (BeaconBrightness(m_PlayerX, m_PlayerY) > 0.0f)
	{
		m_Oil = std::min(1.0f, m_Oil + deltaSeconds / 12.0f);
	}
	else
	{
		m_Oil = std::max(0.0f, m_Oil - deltaSeconds / kOilSeconds);
	}

	m_Light.BeginFrame();

	// 기름이 줄면 반경도 줄어든다. 꺼지기 전에 먼저 세계가 좁아진다.
	if (m_Oil > 0.0f)
	{
		const float radius = kLanternRadius * (0.35f + 0.65f * m_Oil);
		m_Light.AddLight(m_PlayerX, m_PlayerY, radius, true);
	}

	for (const StaticLight& fire : m_Fires)
	{
		m_Light.AddLight(fire.x, fire.y, fire.radius, false);
	}

	// 여기서부터 밝기 질의는 이번 프레임의 등불을 반영한다.
	// 굳은 사람과 상호작용 판정이 한 프레임 늦지 않도록 조명을 먼저 세운다.
	UpdateSleepers(deltaSeconds);
	HandleInteraction(deltaSeconds, input);

	// 동행자는 궤적을 따라 걷고, 어둠 속에 오래 있으면 다시 굳어간다.
	if (m_Companion.IsAwake())
	{
		float targetX = 0.0f;
		float targetY = 0.0f;
		if (CompanionTarget(targetX, targetY))
		{
			m_Companion.MoveTowards(targetX, targetY, deltaSeconds);
		}
		m_Companion.UpdateErosion(deltaSeconds,
		                          TotalBrightness(m_Companion.X(), m_Companion.Y()));
	}

	m_Light.Decay(deltaSeconds);
}

void Game::DrawTower(Renderer* renderer, const Regions::Region& region)
{
	const float worldX = TileToWorld(region.towerTileX);
	const float worldY = TileToWorld(region.towerTileY);
	const float x = ToRenderX(worldX);
	const float y = ToRenderY(worldY);

	// 아직 침묵한 종탑도 연기 너머로는 보인다. 무한 맵에서 갈 곳이 된다.
	// 울린 종탑은 돌이 종의 불빛을 받아 따뜻해진다.
	const float shade = region.rung ? 1.0f : 0.55f;
	const float warm = region.rung ? 0.22f : 0.0f;
	const Color stone = Blend(kStone, kFlame, warm, shade);
	const Color stoneDark = Blend(kStoneDark, kFlame, warm * 0.6f, shade);
	const Color groove = { stoneDark.r, stoneDark.g, stoneDark.b, 0.5f };
	const Color side = { stoneDark.r, stoneDark.g, stoneDark.b, 0.55f };

	DrawShadow(renderer, x, y - 1.0f, 26.0f, 0.45f);

	// 기단과 몸체. 오른쪽에 그늘을 한 줄 넣어 두께를 흉내낸다.
	renderer->DrawRoundRect(x, y + 4.0f, 38.0f, 9.0f, 2.0f, stoneDark);
	renderer->DrawRoundRect(x, y + 31.0f, 28.0f, 46.0f, 1.5f, stone);
	renderer->DrawRoundRect(x + 9.5f, y + 31.0f, 9.0f, 46.0f, 1.0f, side);
	for (int i = 0; i < 3; ++i)
	{
		renderer->DrawRoundRect(x, y + 25.0f + i * 9.0f, 28.0f, 1.0f, 0.5f, groove);   // 줄눈
	}

	// 아치형 문
	renderer->DrawRoundRect(x - 2.0f, y + 15.5f, 9.0f, 15.0f, 4.5f, Tint(kCrow));

	// 종루 — 뚫린 창 안에 종이 걸려 있다. 울린 종루는 안이 불빛으로 차 있다.
	renderer->DrawRoundRect(x, y + 47.0f, 17.0f, 15.0f, 7.5f,
	                        region.rung ? Blend(kWoodDark, kFlame, 0.35f) : Tint(kCrow));

	// 종. 당기는 중이면 당긴 만큼 크게 흔들린다.
	const bool ringingHere = m_RingProgress > 0.0f
	                      && Distance(m_PlayerX, m_PlayerY, worldX, worldY) <= kTowerReach;
	const float swing = ringingHere ? std::sin(m_Time * 9.0f) * 0.35f * m_RingProgress : 0.0f;
	const float bellX = x + std::sin(swing) * 3.0f;
	const float bellGlow = region.rung ? kGlowBell : 0.0f;
	const Color bell = region.rung ? Blend(kBronze, kFlame, 0.45f) : Tint(kBronze, shade * 0.6f);
	renderer->DrawEllipse(bellX, y + 47.5f, 4.8f, 5.2f, bell, swing, bellGlow);
	renderer->DrawRoundRect(bellX + std::sin(swing), y + 43.0f, 11.0f, 2.4f, 1.0f, bell, swing, bellGlow);

	// 처마, 지붕, 꼭대기 장식
	renderer->DrawRoundRect(x, y + 56.0f, 34.0f, 4.0f, 1.0f, stoneDark);
	renderer->DrawTriangle(x, y + 68.5f, 37.0f, 22.0f, Tint(kRoof, shade));
	renderer->DrawRoundRect(x, y + 83.0f, 2.2f, 9.0f, 1.0f, stoneDark);
	renderer->DrawRoundRect(x, y + 84.5f, 7.0f, 2.2f, 1.0f, stoneDark);

	if (!region.rung)
	{
		// 까마귀 떼. 아직 침묵한 종탑의 표식이다. 지붕 비탈과 처마 끝에 앉는다.
		const float perchX[4] = { -16.0f, -10.0f, 7.0f, 15.5f };
		const float facing[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
		const float seed = region.regionX * 0.37f + region.regionY * 0.71f;

		for (int i = 0; i < 4; ++i)
		{
			const float along = std::fabs(perchX[i]);
			const float perchY = (along > 14.5f) ? 58.0f : 57.5f + (1.0f - along / 18.5f) * 22.0f;
			DrawCrow(renderer, x + perchX[i], y + perchY, facing[i], m_Time, seed + i * 1.7f);
		}
	}
	else
	{
		// 종탑에 남아 종을 울리는 사람. 종에서 내려온 밧줄을 쥐고 있다.
		FigureLook keeper;
		keeper.body = Tint(kTunic);
		keeper.skin = Tint(kLiving);
		keeper.hair = Tint(kHair);
		keeper.hooded = false;
		keeper.posture = 1.0f;
		keeper.facing = -1.0f;
		keeper.bob = 0.0f;

		float handX = x;
		float handY = y;
		DrawFigure(renderer, x + 27.0f, y - 3.0f, keeper, &handX, &handY);
		renderer->DrawSegment(bellX, y + 42.0f, handX, handY, 1.2f, Tint(kRope));
	}

	// 종을 당기는 중이면 종탑 위로 불씨 눈금이 찬다.
	if (ringingHere)
	{
		const int pips = 8;
		const int filled = static_cast<int>(m_RingProgress * pips + 0.5f);
		for (int i = 0; i < pips; ++i)
		{
			const bool lit = i < filled;
			renderer->DrawCircle(x - 28.0f + i * 8.0f, y + 98.0f, lit ? 3.0f : 2.2f,
			                     lit ? Tint(kFlame) : Tint(kStoneDark, 0.8f),
			                     lit ? kGlowGauge : 0.0f);
		}
	}
}

void Game::DrawSleeper(Renderer* renderer, const Regions::Region& region)
{
	const float worldX = TileToWorld(region.sleeperTileX);
	const float x = ToRenderX(worldX);
	const float y = ToRenderY(TileToWorld(region.sleeperTileY));
	const float warmth = region.warmth;

	// 굳어 있을 때는 돌빛으로 웅크려 있다. 생기가 돌수록 색이 돌아오고 몸을 편다.
	// 말을 걸어 따라나서기 전까지는 완전히 일어서지 않는다.
	FigureLook look;
	look.body = Blend(kFrozen, kTunic, warmth);
	look.skin = Blend(kFrozen, kLiving, warmth);
	look.hair = Blend(kFrozen, kHair, warmth);
	look.hooded = false;
	look.posture = warmth * 0.85f;
	look.facing = (m_PlayerX >= worldX) ? 1.0f : -1.0f;
	look.bob = 0.0f;
	DrawFigure(renderer, x, y, look);

	// 생기가 다 차면 머리 위에 불씨가 떠오른다. 말을 걸 수 있다는 뜻이다.
	if (warmth >= 1.0f)
	{
		const float hover = y + 40.0f + std::sin(m_Time * 3.0f) * 1.5f;
		renderer->DrawRoundRect(x, hover, 4.6f, 4.6f, 1.0f, Tint(kFlame), 0.7854f, kGlowSpark);
		renderer->DrawCircle(x, hover, 1.2f, Tint(kFlameCore), kGlowSpark * 1.5f);
	}
}

void Game::DrawCompanion(Renderer* renderer)
{
	const float alive = 1.0f - m_Companion.Darkness();
	const float x = ToRenderX(m_Companion.X());
	const float y = ToRenderY(m_Companion.Y());

	// 침식도가 그대로 색과 자세가 된다. 굳어갈수록 색이 빠지고 몸이 움츠러든다.
	FigureLook look;
	look.body = Blend(kFrozen, kTunic, alive);
	look.skin = Blend(kFrozen, kLiving, alive);
	look.hair = Blend(kFrozen, kHair, alive);
	look.hooded = false;
	look.posture = Mix(0.35f, 1.0f, alive);
	look.facing = (m_PlayerX >= m_Companion.X()) ? 1.0f : -1.0f;

	const float gap = Distance(m_PlayerX, m_PlayerY, m_Companion.X(), m_Companion.Y());
	const bool walking = m_PlayerMoving && !m_Companion.IsFrozenAgain() && gap > kFollowLag * 0.6f;
	look.bob = walking ? std::fabs(std::sin(m_Time * 9.0f + 1.3f)) * 1.4f : 0.0f;

	DrawFigure(renderer, x, y, look);
}

void Game::DrawPlayer(Renderer* renderer)
{
	const float x = ToRenderX(m_PlayerX);
	const float y = ToRenderY(m_PlayerY);

	// 기름이 마르면 등불이 꺼진다.
	const float lanternGlow = (m_Oil > 0.0f) ? 0.35f + 0.65f * m_Oil : 0.0f;

	FigureLook look;
	look.body = Tint(kCloak);
	look.skin = Tint(kLiving);
	look.hair = Tint(kHood);
	look.hooded = true;
	look.posture = 1.0f;
	look.facing = m_FacingX;
	look.bob = m_PlayerMoving ? std::fabs(std::sin(m_Time * 10.0f)) * 1.6f : 0.0f;

	float handX = x;
	float handY = y;
	DrawFigure(renderer, x, y, look, &handX, &handY);

	// 걸으면 크게, 서 있으면 숨 쉬듯 조금 흔들린다.
	const float swing = m_PlayerMoving
		? std::sin(m_Time * 10.0f) * 0.28f
		: std::sin(m_Time * 1.6f) * 0.06f;
	DrawLantern(renderer, handX, handY, swing, lanternGlow);
}

void Game::DrawHud(Renderer* renderer)
{
	// 글자를 그릴 수단이 아직 없으므로 모양으로 표시한다.
	// 기획서의 "UI 없이 색으로 읽히게" 방침과도 맞는다.
	const float left = -m_WindowWidth * 0.5f + 22.0f;
	const float bottom = -m_WindowHeight * 0.5f + 20.0f;

	// 등불 기름 — 기름방울 16개
	const int oilDrops = 16;
	const int oilFilled = static_cast<int>(m_Oil * oilDrops + 0.5f);
	for (int i = 0; i < oilDrops; ++i)
	{
		const Color color = (i < oilFilled) ? Tint(kFlame) : Tint(kFlame, 0.16f);
		const float dropX = left + i * 11.0f;
		renderer->DrawCircle(dropX, bottom, 3.4f, color);
		renderer->DrawTriangle(dropX, bottom + 3.6f, 4.2f, 4.4f, color);
	}

	// 남은 화톳불 — 장작 위의 작은 불꽃
	for (int i = 0; i < 3; ++i)
	{
		const float shade = (i < m_FiresLeft) ? 1.0f : 0.16f;
		const float fireX = left + 196.0f + i * 17.0f;
		renderer->DrawRoundRect(fireX, bottom - 5.0f, 11.0f, 2.6f, 1.2f, Tint(kWood, shade));
		renderer->DrawTriangle(fireX, bottom + 2.0f, 9.0f, 13.0f, Tint(kEmber, shade));
		renderer->DrawTriangle(fireX, bottom + 0.5f, 5.0f, 8.0f, Tint(kFlame, shade));
	}

	// 동행자의 얼굴. 색이 빠질수록 어둠에 잠식된 것이다.
	if (m_Companion.IsAwake())
	{
		const float alive = 1.0f - m_Companion.Darkness();
		const float faceX = left + 264.0f;
		renderer->DrawCircle(faceX, bottom + 1.0f, 7.5f, Tint(kCrow, 1.0f, 0.6f));
		renderer->DrawCircle(faceX, bottom + 1.0f, 5.5f, Blend(kFrozen, kLiving, alive));
		renderer->DrawEllipse(faceX - 0.8f, bottom + 3.8f, 5.6f, 3.0f, Blend(kFrozen, kHair, alive));
	}
}

void Game::Render(Renderer* renderer)
{
	const float halfWidth = m_WindowWidth * 0.5f;
	const float halfHeight = m_WindowHeight * 0.5f;
	const float worldLeft = m_CameraX - halfWidth;
	const float worldRight = m_CameraX + halfWidth;
	const float worldTop = m_CameraY - halfHeight;
	const float worldBottom = m_CameraY + halfHeight;

	const int firstTileX = FloorToInt(worldLeft / kTileSize);
	const int lastTileX  = FloorToInt(worldRight / kTileSize);
	const int firstTileY = FloorToInt(worldTop / kTileSize);
	const int lastTileY  = FloorToInt(worldBottom / kTileSize);

	// ── 1. 안개 층 ──────────────────────────────────────────────
	// 배경색 자체가 연기다. 바닥만 아주 어둡게 얹어 윤곽을 남긴다.
	// 되찾은 구역 안에서는 이 하한이 종탑의 밝기까지 올라가 벽도 모습을 갖는다.
	for (int tileY = firstTileY; tileY <= lastTileY; ++tileY)
	{
		for (int tileX = firstTileX; tileX <= lastTileX; ++tileX)
		{
			const float worldX = TileToWorld(tileX);
			const float worldY = TileToWorld(tileY);
			const float brightness = std::max(kFogBrightness, BeaconBrightness(worldX, worldY));
			const bool wall = m_World.IsWall(tileX, tileY);

			// 연기 속 벽은 배경과 같은 색이라 그릴 필요가 없다.
			if (wall && brightness <= kFogBrightness)
			{
				continue;
			}

			const float* base = wall ? kWallColor : kFloorColor;
			renderer->DrawSolidRect(ToRenderX(worldX), ToRenderY(worldY),
			                        0.0f, static_cast<float>(kTileSize),
			                        base[0] * brightness,
			                        base[1] * brightness,
			                        base[2] * brightness,
			                        1.0f);
		}
	}

	// ── 2. 조명 층 ──────────────────────────────────────────────
	// 안개나 종탑 불빛보다 밝은 칸만 다시 그린다.
	const int firstCellX = m_Light.CellOf(worldLeft);
	const int lastCellX  = m_Light.CellOf(worldRight);
	const int firstCellY = m_Light.CellOf(worldTop);
	const int lastCellY  = m_Light.CellOf(worldBottom);

	for (int cellY = firstCellY; cellY <= lastCellY; ++cellY)
	{
		for (int cellX = firstCellX; cellX <= lastCellX; ++cellX)
		{
			const float dynamic = m_Light.Brightness(cellX, cellY);
			if (dynamic < kFogBrightness)
			{
				continue;
			}

			const float worldX = (cellX + 0.5f) * kLightCell;
			const float worldY = (cellY + 0.5f) * kLightCell;

			const float floorLevel = std::max(kFogBrightness,
			                                  BeaconBrightness(worldX, worldY));
			if (dynamic <= floorLevel + 0.01f)
			{
				continue;
			}

			const bool wall = IsWallAtWorld(worldX, worldY);
			const float* base = wall ? kWallColor : kFloorColor;

			// 불이 직접 비추는 곳은 따뜻하게, 남은 발자국은 차갑게 식어간다.
			const float warmth = m_Light.Glow(cellX, cellY) * 0.35f;

			renderer->DrawSolidRect(
				ToRenderX(worldX), ToRenderY(worldY), 0.0f, kLightCell,
				Mix(base[0], kFlame[0], warmth) * dynamic,
				Mix(base[1], kFlame[1], warmth) * dynamic,
				Mix(base[2], kFlame[2], warmth) * dynamic,
				1.0f);
		}
	}

	// ── 3. 오브젝트 ─────────────────────────────────────────────
	// 발밑 y가 작은 것(화면 위쪽 = 뒤쪽)부터 그려, 앞에 선 것이 뒤의 것을 가린다.
	// 오브젝트는 발밑에서 위로 자라므로, 발이 화면 아래 밖에 있어도 머리는 보일 수 있다.
	const float marginX = 48.0f;
	const float tallest = 110.0f;   // 가장 키 큰 것(종탑)의 높이
	auto inView = [&](float wx, float wy)
	{
		return wx > worldLeft - marginX && wx < worldRight + marginX
		    && wy > worldTop - 16.0f && wy < worldBottom + tallest;
	};

	m_Drawables.clear();

	for (size_t i = 0; i < m_NearbyRegions.size(); ++i)
	{
		const Regions::Region& region = m_NearbyRegions[i];

		const float towerX = TileToWorld(region.towerTileX);
		const float towerY = TileToWorld(region.towerTileY);
		if (inView(towerX, towerY))
		{
			m_Drawables.push_back({ towerY, kDrawTower, static_cast<int>(i) });
		}

		// 굳어 있는 사람은 숨어 있지 않다. 연기 너머로도 형체가 보인다.
		if (!region.woken)
		{
			const float sleeperX = TileToWorld(region.sleeperTileX);
			const float sleeperY = TileToWorld(region.sleeperTileY);
			if (inView(sleeperX, sleeperY))
			{
				m_Drawables.push_back({ sleeperY, kDrawSleeper, static_cast<int>(i) });
			}
		}
	}

	// 사물은 사람과 달리 안개 속에서 보이지 않는다. 불을 가져가야 드러난다.
	m_VisibleProps.clear();
	m_World.CollectProps(firstTileX - 1, firstTileY, lastTileX + 1, lastTileY + 4, m_VisibleProps);
	for (size_t i = 0; i < m_VisibleProps.size(); ++i)
	{
		const float worldX = TileToWorld(m_VisibleProps[i].tileX);
		const float worldY = TileToWorld(m_VisibleProps[i].tileY);
		if (inView(worldX, worldY) && TotalBrightness(worldX, worldY) >= kRevealAt)
		{
			m_Drawables.push_back({ worldY, kDrawProp, static_cast<int>(i) });
		}
	}

	if (m_Companion.IsAwake())
	{
		m_Drawables.push_back({ m_Companion.Y(), kDrawCompanion, 0 });
	}

	for (size_t i = 0; i < m_Fires.size(); ++i)
	{
		if (inView(m_Fires[i].x, m_Fires[i].y))
		{
			m_Drawables.push_back({ m_Fires[i].y, kDrawFire, static_cast<int>(i) });
		}
	}

	m_Drawables.push_back({ m_PlayerY, kDrawPlayer, 0 });

	std::stable_sort(m_Drawables.begin(), m_Drawables.end(),
	                 [](const Drawable& a, const Drawable& b) { return a.sortY < b.sortY; });

	for (const Drawable& item : m_Drawables)
	{
		switch (item.kind)
		{
		case kDrawTower:
			DrawTower(renderer, m_NearbyRegions[item.index]);
			break;

		case kDrawSleeper:
			DrawSleeper(renderer, m_NearbyRegions[item.index]);
			break;

		case kDrawProp:
		{
			const World::Prop& prop = m_VisibleProps[item.index];
			const float worldX = TileToWorld(prop.tileX);
			const float worldY = TileToWorld(prop.tileY);

			// 밝기 문턱을 막 넘긴 것은 희미하게, 충분히 밝으면 또렷하게 드러난다.
			const float reveal = std::min(1.0f, (TotalBrightness(worldX, worldY) - kRevealAt) / 0.35f);
			DrawProp(renderer, ToRenderX(worldX), ToRenderY(worldY), prop, std::max(0.0f, reveal));
			break;
		}

		case kDrawCompanion:
			DrawCompanion(renderer);
			break;

		case kDrawFire:
			DrawBonfire(renderer, ToRenderX(m_Fires[item.index].x), ToRenderY(m_Fires[item.index].y),
			            m_Time, item.index * 2.3f);
			break;

		case kDrawPlayer:
			DrawPlayer(renderer);
			break;
		}
	}
}

void Game::RenderHud(Renderer* renderer)
{
	DrawHud(renderer);
}
