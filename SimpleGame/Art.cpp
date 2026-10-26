#include "stdafx.h"
#include "Art.h"

#include <algorithm>
#include <cmath>

namespace Art
{
	extern const int kTileSize   = World::kTilePixels;

	// 밝기 격자는 타일보다 촘촘하게 잡는다. 조명이 타일 단위로 각지지 않는다.
	extern const float kLightCell  = 16.0f;

	// ── 안개 ────────────────────────────────────────────────────
	// 세계는 검은 연기에 덮여 있다. 완전한 암흑이 아니라서 지형의 윤곽은
	// 겨우 알아볼 수 있지만, 그 안에 무엇이 있는지는 불을 가져가야 보인다.
	extern const float kFogBrightness = 0.22f;

	// 오브젝트와 사람이 모습을 드러내기 시작하는 밝기.
	// 안개 밝기보다 확실히 높아야 "불로 밝혀야 보인다"가 성립한다.
	extern const float kRevealAt = 0.30f;

	// ── 등불 ────────────────────────────────────────────────────
	extern const float kLanternRadius = 125.0f;   // 손에 든 불이 닿는 거리. 좁아야 어둠이 가깝다
	extern const float kOilSeconds = 80.0f;      // 가득 찬 기름으로 버티는 시간. 숲길 둘셋을 지나면 바닥난다

	// ── 화톳불 ──────────────────────────────────────────────────
	extern const float kFireRadius = 130.0f;

	// ── 굳은 사람 ───────────────────────────────────────────────
	// 어둠에 잡힌 사람은 숨어 있지 않다. 연기 너머로도 형체가 보인다.
	// 다만 죽은 돌처럼 서 있을 뿐이라, 불을 들이대고 버텨야 생기가 돈다.
	extern const float kReviveSeconds = 2.0f;   // 불을 비춘 채 버텨야 하는 시간
	extern const float kChillSeconds = 4.0f;    // 등불이 떠나면 다시 식는 시간

	// ── 종탑 ────────────────────────────────────────────────────
	extern const float kRingSeconds = 4.5f;     // 종이 제대로 울리기까지
	extern const float kTowerReach = 46.0f;     // 플레이어가 밧줄에 닿는 거리
	extern const float kCompanionReach = 95.0f; // 동행자가 함께 당길 수 있는 거리
	extern const float kWakeReach = 36.0f;      // 굳은 사람에게 말을 거는 거리

	// 울린 종탑이 밝히는 범위와 밝기. 구역 한 변의 절반쯤이라
	// 구역 하나를 되찾으면 그 일대가 안전해진다.
	extern const float kBeaconBrightness = 0.62f;

	// ── 궤적 ────────────────────────────────────────────────────
	extern const float kFollowLag = 46.0f;      // 동행자가 뒤처지는 거리
	extern const float kPathKeep = 420.0f;      // 궤적을 얼마나 오래 들고 있을지
	extern const float kPathStep = 3.0f;        // 이만큼 움직여야 점을 하나 찍는다

	// ── 색 ──────────────────────────────────────────────────────
	// 채도를 낮게 잡아, 불의 호박색만 따뜻하게 남긴다.
	// 땅. 역병이 지나간 마을이라 풀빛도 흙빛도 가라앉아 있다.
	extern const float kGrassColor[3] = { 0.20f, 0.25f, 0.17f };
	extern const float kPathColor[3]  = { 0.36f, 0.29f, 0.21f };
	extern const float kPlazaColor[3] = { 0.33f, 0.33f, 0.35f };
	extern const float kCanopy[3]      = { 0.07f, 0.12f, 0.08f };
	extern const float kCanopyLight[3] = { 0.11f, 0.18f, 0.11f };
	extern const float kFlame[3]      = { 0.98f, 0.72f, 0.36f };
	extern const float kRubble[3]     = { 0.42f, 0.38f, 0.31f };
	extern const float kSupply[3]     = { 0.45f, 0.62f, 0.52f };
	extern const float kStone[3]      = { 0.34f, 0.36f, 0.40f };
	extern const float kCrow[3]       = { 0.02f, 0.02f, 0.03f };
	extern const float kLiving[3]     = { 0.92f, 0.74f, 0.58f };   // 깨어난 사람의 살색
	extern const float kFrozen[3]     = { 0.10f, 0.10f, 0.13f };   // 굳어버린 사람. 연기보다 겨우 밝다

	// ── 발광 ────────────────────────────────────────────────────
	// 스스로 빛나는 것은 DrawEmissiveRect로 그린다. 불꽃 자체는 불꽃 색 그대로
	// 그리고, 이 세기만큼의 빛을 발광 버퍼에 따로 남긴다. 후처리 Bloom이
	// 그 빛을 번지게 하므로, 빛은 사각형이 아니라 번짐으로 보인다.
	//
	// 번짐의 크기는 "세기 x 도형 면적"에 비례한다. 작은 도형일수록 세기를 높게.
	extern const float kGlowLantern = 8.0f;    // 등불 유리창. 심지는 1.8배. 기름이 줄면 함께 약해진다
	extern const float kGlowFire    = 5.0f;    // 화톳불 불꽃 가운데 겹. 바깥 겹은 절반, 심지는 1.8배
	extern const float kGlowBell    = 8.0f;    // 울린 종. 멀리서 보이는 표식
	extern const float kGlowSpark   = 9.0f;    // 말을 걸 수 있게 된 사람 머리 위 불씨
	extern const float kGlowGauge   = 5.0f;    // 종을 당길 때 차오르는 눈금
	extern const float kGlowWindow  = 2.5f;    // 되찾은 마을의 집 창문. 사람이 돌아와 불을 켰다

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

	float VillageWallDistance()
	{
		return static_cast<float>((World::kVillageCenter - World::kWallLow) * kTileSize);   // 512
	}

	float VillageLane()
	{
		return static_cast<float>((World::kVillageCenter - World::kLaneLow) * kTileSize);   // 480
	}

	float BeaconInner()
	{
		return VillageWallDistance() - 42.0f;    // 470 — 울타리 바로 안쪽까지 한껏 밝다
	}

	float BeaconRadius()
	{
		return VillageWallDistance() + 128.0f;   // 640 — 문 밖 네 칸까지 새어 나간다
	}

	// ══ 오브젝트 도안 ════════════════════════════════════════════
	// 오브젝트는 전부 발밑이 기준점이고 위로 자란다(렌더러 좌표는 y가 위).
	// 발밑 y로 정렬해 그리므로 화면 아래쪽에 선 것이 앞에 온다 — 2.5D의 기본이다.

	extern const float kCloak[3]     = { 0.24f, 0.21f, 0.18f };   // 플레이어 망토
	extern const float kHood[3]      = { 0.15f, 0.14f, 0.13f };
	extern const float kTunic[3]     = { 0.38f, 0.31f, 0.24f };   // 깨어난 사람의 옷
	extern const float kHair[3]      = { 0.19f, 0.14f, 0.10f };
	extern const float kMetal[3]     = { 0.32f, 0.30f, 0.27f };
	extern const float kWood[3]      = { 0.34f, 0.23f, 0.14f };
	extern const float kWoodDark[3]  = { 0.20f, 0.14f, 0.09f };
	extern const float kEmber[3]     = { 1.00f, 0.42f, 0.14f };
	extern const float kFlameCore[3] = { 1.00f, 0.92f, 0.70f };
	extern const float kStoneDark[3] = { 0.21f, 0.22f, 0.25f };
	extern const float kRoof[3]      = { 0.17f, 0.15f, 0.17f };
	extern const float kBronze[3]    = { 0.58f, 0.44f, 0.22f };
	extern const float kRope[3]      = { 0.45f, 0.38f, 0.26f };

	// 집
	extern const float kPlaster[3]    = { 0.47f, 0.41f, 0.33f };   // 회반죽 벽
	extern const float kTimber[3]     = { 0.22f, 0.15f, 0.10f };   // 목조 뼈대
	extern const float kRoofTile[3]   = { 0.36f, 0.20f, 0.15f };   // 붉은 기와
	extern const float kRoofSlate[3]  = { 0.24f, 0.25f, 0.29f };   // 청석 지붕
	extern const float kThatch[3]     = { 0.42f, 0.34f, 0.20f };   // 초가
	extern const float kWindowDark[3] = { 0.05f, 0.05f, 0.07f };

	// 나무
	extern const float kLeaf[3]       = { 0.17f, 0.25f, 0.15f };
	extern const float kLeafLight[3]  = { 0.25f, 0.34f, 0.20f };
	extern const float kLeafDry[3]    = { 0.33f, 0.27f, 0.16f };   // 병든 잎
	extern const float kPine[3]       = { 0.12f, 0.21f, 0.16f };
	extern const float kBark[3]       = { 0.24f, 0.17f, 0.11f };
	extern const float kDeadWood[3]   = { 0.30f, 0.27f, 0.24f };

	// 미니맵 — 어두운 화면 구석에서도 한눈에 구분되도록 실제 색보다 밝게 잡는다
	extern const float kMapGrass[3]   = { 0.20f, 0.27f, 0.17f };
	extern const float kMapPath[3]    = { 0.47f, 0.38f, 0.26f };
	extern const float kMapPlaza[3]   = { 0.46f, 0.46f, 0.48f };
	extern const float kMapHouse[3]   = { 0.62f, 0.34f, 0.24f };
	extern const float kMapTree[3]    = { 0.11f, 0.19f, 0.12f };
	extern const float kMapFrozen[3]  = { 0.62f, 0.66f, 0.76f };
	extern const float kMapTower[3]   = { 0.60f, 0.63f, 0.70f };
	extern const float kMapRubble[3]  = { 0.58f, 0.55f, 0.50f };
	extern const float kMapSupply[3]  = { 0.40f, 0.80f, 0.64f };
	extern const float kMapBarrel[3]  = { 0.72f, 0.50f, 0.28f };

	extern const int kMapTiles = 96;              // 지도 이미지 한 변의 타일 수
	extern const float kMapPixelsPerTile = 2.0f;    // 지도에서 한 칸이 차지하는 화면 픽셀
	extern const float kMapViewSize = 184.0f;       // 화면에 보이는 지도 한 변(px)


	// 전투
	extern const float kShadeBody[3]  = { 0.045f, 0.045f, 0.060f };   // 삼켜진 자들의 몸. 연기보다 조금 짙다
	extern const float kShadeSkin[3]  = { 0.075f, 0.075f, 0.095f };
	extern const float kWardenRobe[3] = { 0.040f, 0.035f, 0.052f };
	extern const float kRust[3]       = { 0.36f, 0.20f, 0.12f };
	extern const float kEyeRed[3]     = { 1.00f, 0.28f, 0.16f };
	extern const float kEyePale[3]    = { 0.70f, 0.82f, 0.95f };
	extern const float kSoulColor[3]  = { 0.55f, 0.78f, 1.00f };
	extern const float kHealRed[3]    = { 0.74f, 0.14f, 0.14f };
	extern const float kSteel[3]      = { 0.82f, 0.84f, 0.90f };
	extern const float kGold[3]       = { 0.95f, 0.72f, 0.28f };
	extern const float kMagnetRed[3]  = { 0.80f, 0.16f, 0.16f };
	extern const float kOrbCore[3]    = { 0.08f, 0.03f, 0.12f };
	extern const float kOrbRim[3]     = { 0.62f, 0.30f, 0.90f };

	extern const float kGlowEye    = 4.0f;    // 어둠 속에서는 눈빛만 먼저 보인다
	extern const float kGlowEmber  = 7.0f;    // 날아가는 불씨
	extern const float kGlowSoul   = 3.5f;

	// 미니맵 표식 종류
	extern const int kMarkTowerSilent = 0;
	extern const int kMarkTowerRung   = 1;
	extern const int kMarkSleeper     = 2;
	extern const int kMarkCompanion   = 3;
	extern const int kMarkFire        = 4;
	extern const int kMarkSupply      = 5;
	extern const int kMarkBarrel      = 6;
	extern const int kMarkRubble      = 7;
	extern const int kMarkWarden      = 8;
	extern const int kMarkEnemy       = 9;
	extern const int kMarkParty       = 10;

	Color Tint(const float c[3], float shade, float alpha)
	{
		Color color = { c[0] * shade, c[1] * shade, c[2] * shade, alpha };
		return color;
	}

	Color Blend(const float a[3], const float b[3], float t, float shade)
	{
		Color color = { Mix(a[0], b[0], t) * shade,
		                Mix(a[1], b[1], t) * shade,
		                Mix(a[2], b[2], t) * shade,
		                1.0f };
		return color;
	}

	// 발밑 그림자. 경계를 흐려 바닥에 붙어 있는 느낌을 준다.
	void DrawShadow(Renderer* r, float x, float y, float radiusX, float alpha)
	{
		const Color shadow = { 0.0f, 0.0f, 0.0f, alpha };
		r->DrawEllipse(x, y, radiusX, radiusX * 0.36f, shadow, 0.0f, 0.0f, 2.5f);
	}


	// 사람 한 명. 발밑 (x, y)에서 위로 약 32px. 앞으로 뻗은 손의 위치를 돌려준다.
	void DrawFigure(Renderer* r, float x, float y, const FigureLook& look,
	                float* outHandX, float* outHandY)
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

		if (prop.kind == World::kPropBarrel)
		{
			// 나무통 — 쇠테 두 줄
			r->DrawRoundRect(x, y + 8.0f, 12.0f, 15.0f, 4.0f, Tint(kWood, shade));
			r->DrawRoundRect(x, y + 4.0f, 12.5f, 1.8f, 0.8f, Tint(kMetal, shade));
			r->DrawRoundRect(x, y + 12.0f, 12.5f, 1.8f, 0.8f, Tint(kMetal, shade));
			r->DrawEllipse(x, y + 15.0f, 5.5f, 2.2f, Tint(kWoodDark, shade));
		}
		else if (prop.kind == World::kPropSupply)
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

	Color LitTint(const float c[3], float shade, float warmth)
	{
		Color color = { Mix(c[0], kFlame[0], warmth) * shade,
		                Mix(c[1], kFlame[1], warmth) * shade,
		                Mix(c[2], kFlame[2], warmth) * shade,
		                1.0f };
		return color;
	}

	unsigned char ToByte(float value)
	{
		return static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, value)) * 255.0f + 0.5f);
	}

	// 땅 한 칸의 색. 칸마다 조금씩 흔들어 격자가 드러나지 않게 한다.
	// 집과 나무가 선 자리의 땅은 풀밭이다.
	void GroundColor(unsigned char tile, int tileX, int tileY, float out[3])
	{
		const unsigned int hash = PropHash(tileX, tileY);
		const float jitter = (static_cast<float>(hash & 0xFFu) / 255.0f - 0.5f) * 0.045f;

		const float* base = kGrassColor;
		float shade = 1.0f;
		if (tile == World::kPath)
		{
			base = kPathColor;
		}
		else if (tile == World::kPlaza)
		{
			// 돌바닥은 한 칸씩 엇갈려 판석이 깔린 것처럼 보이게 한다
			base = kPlazaColor;
			shade = ((tileX + tileY) & 1) ? 1.0f : 0.88f;
		}
		else if (tile == World::kWoods)
		{
			// 깊은 숲의 바닥. 위에 수관을 따로 그리므로 어둡게 깐다.
			base = kCanopy;
			shade = 0.55f + static_cast<float>((hash >> 8) & 0xFFu) / 255.0f * 0.25f;
		}
		else if (tile == World::kWall)
		{
			base = kPathColor;   // 울타리 밑은 다져진 흙
			shade = 0.8f;
		}

		for (int i = 0; i < 3; ++i)
		{
			out[i] = std::max(0.0f, base[i] * shade + jitter);
		}
	}

	// 집 한 채. (left, bottom)은 터의 왼쪽 아래 모서리(렌더러 좌표)이고 앞면이 화면 아래를 향한다.
	// 앞벽이 서고 지붕이 터 전체를 덮으며 뒤로 넘어가, 위에서 비스듬히 내려다본 모습이 된다.
	//   shade  : 안개와 불빛에 따른 밝기
	//   warmth : 등불이 비추는 따뜻한 색조
	//   lived  : 되찾은 마을이라 사람이 돌아와 창에 불이 켜졌는가
	void DrawHouse(Renderer* r, float left, float bottom, const World::House& house,
	               float shade, float warmth, bool lived, float time)
	{
		const float width = house.width * static_cast<float>(kTileSize);
		const float depth = house.depth * static_cast<float>(kTileSize);
		const float cx = left + width * 0.5f;
		const float wallHeight = 40.0f;
		const float wallWidth = width - 6.0f;
		const float ruin = house.ruined ? 0.72f : 1.0f;

		const Color shadow = { 0.0f, 0.0f, 0.0f, 0.42f };
		r->DrawEllipse(cx, bottom, width * 0.56f, 7.0f, shadow, 0.0f, 0.0f, 4.0f);

		// ── 앞벽 ──
		const Color timber = LitTint(kTimber, shade, warmth);
		const float wallCenterY = bottom + wallHeight * 0.5f;
		const float wallLeft = cx - wallWidth * 0.5f;
		const float wallRight = cx + wallWidth * 0.5f;

		r->DrawRoundRect(cx, wallCenterY, wallWidth, wallHeight, 1.5f, LitTint(kPlaster, shade * ruin, warmth));
		r->DrawRoundRect(wallLeft + 2.0f, wallCenterY, 4.0f, wallHeight, 1.0f, timber);
		r->DrawRoundRect(wallRight - 2.0f, wallCenterY, 4.0f, wallHeight, 1.0f, timber);
		r->DrawRoundRect(cx, bottom + 2.0f, wallWidth, 4.0f, 1.0f, timber);
		r->DrawRoundRect(cx, bottom + wallHeight - 2.0f, wallWidth, 4.0f, 1.0f, timber);
		if (house.width >= 4)
		{
			r->DrawRoundRect(cx, wallCenterY, 3.0f, wallHeight, 1.0f, timber);
		}

		// ── 문 ──
		const float doorX = cx + house.door * wallWidth * 0.28f;
		r->DrawRoundRect(doorX, bottom + 13.5f, 15.0f, 27.0f, 6.0f, timber);
		r->DrawRoundRect(doorX, bottom + 12.5f, 11.0f, 24.0f, 5.0f,
		                 LitTint(kWoodDark, shade * (house.ruined ? 0.45f : 0.95f), warmth));
		if (house.ruined)
		{
			r->DrawSegment(doorX - 5.0f, bottom + 4.0f, doorX + 4.0f, bottom + 20.0f, 2.0f, timber);
		}
		else
		{
			r->DrawCircle(doorX + 3.0f, bottom + 12.0f, 1.1f, LitTint(kMetal, shade * 1.3f, warmth));
		}

		// ── 창 ── 문과 겹치지 않는 쪽에만 낸다
		for (int side = -1; side <= 1; side += 2)
		{
			const float windowX = cx + side * wallWidth * 0.3f;
			if (std::fabs(windowX - doorX) < 14.0f)
			{
				continue;
			}

			const float windowY = bottom + 25.0f;
			r->DrawRoundRect(windowX, windowY, 12.0f, 12.0f, 1.0f, timber);

			if (lived && !house.ruined)
			{
				// 되찾은 마을의 집에는 사람이 돌아와 창에 불이 켜진다.
				const float flicker = 0.85f + 0.15f * std::sin(time * 2.3f + windowX * 0.1f);
				r->DrawRoundRect(windowX, windowY, 8.0f, 8.0f, 0.5f, Tint(kFlame, flicker),
				                 0.0f, kGlowWindow * flicker);
			}
			else
			{
				r->DrawRoundRect(windowX, windowY, 8.0f, 8.0f, 0.5f, LitTint(kWindowDark, shade, warmth));
			}

			r->DrawRoundRect(windowX, windowY, 1.2f, 8.0f, 0.5f, timber);
			r->DrawRoundRect(windowX, windowY, 8.0f, 1.2f, 0.5f, timber);

			if (house.ruined)
			{
				r->DrawSegment(windowX - 5.0f, windowY - 5.0f, windowX + 5.0f, windowY + 5.0f, 2.2f,
				               LitTint(kWood, shade * 0.9f, warmth));   // 판자로 막은 창
			}
		}

		// ── 지붕 ── 터 전체를 덮는다. 용마루를 경계로 앞쪽 비탈을 어둡게 해 두 면으로 보이게 한다
		const float roofBottom = bottom + wallHeight - 6.0f;
		const float roofTop = bottom + depth + 22.0f;
		const float roofHeight = roofTop - roofBottom;
		const float roofWidth = width + 10.0f;
		const float* roofBase = (house.style == 0) ? kRoofTile : (house.style == 1 ? kRoofSlate : kThatch);

		const Color backSlope = LitTint(roofBase, shade * ruin, warmth);
		const Color frontSlope = LitTint(roofBase, shade * ruin * 0.72f, warmth);
		const Color course = { frontSlope.r * 0.75f, frontSlope.g * 0.75f, frontSlope.b * 0.75f, 0.7f };

		r->DrawRoundRect(cx, (roofBottom + roofTop) * 0.5f, roofWidth, roofHeight, 4.0f, backSlope);
		r->DrawRoundRect(cx, roofBottom + roofHeight * 0.31f, roofWidth, roofHeight * 0.62f, 4.0f, frontSlope);
		r->DrawRoundRect(cx, roofBottom + roofHeight * 0.14f, roofWidth - 3.0f, 1.2f, 0.5f, course);
		r->DrawRoundRect(cx, roofBottom + roofHeight * 0.36f, roofWidth - 3.0f, 1.2f, 0.5f, course);
		r->DrawRoundRect(cx, roofBottom + roofHeight * 0.62f, roofWidth - 4.0f, 3.5f, 1.5f,
		                 LitTint(roofBase, shade * ruin * 0.45f, warmth));                 // 용마루
		r->DrawRoundRect(cx, roofBottom + 1.5f, roofWidth + 2.0f, 3.0f, 1.5f, timber);   // 처마 끝

		// ── 굴뚝 ──
		const float chimneyX = cx + (((house.tileX + house.tileY) & 1) ? 1.0f : -1.0f) * width * 0.28f;
		const float chimneyY = roofBottom + roofHeight * 0.8f;
		r->DrawRoundRect(chimneyX, chimneyY, 8.0f, 14.0f, 1.0f, LitTint(kStone, shade * 0.8f, warmth));

		if (lived && !house.ruined)
		{
			// 굴뚝 연기
			for (int i = 0; i < 3; ++i)
			{
				const float cycle = time * 0.35f + i / 3.0f + house.tileX * 0.13f;
				const float phase = cycle - std::floor(cycle);
				const Color smoke = { 0.36f, 0.35f, 0.34f, 0.26f * (1.0f - phase) };
				r->DrawCircle(chimneyX + std::sin(phase * 5.0f + i) * 3.0f, chimneyY + 9.0f + phase * 22.0f,
				              3.0f + phase * 4.0f, smoke, 0.0f, 1.5f);
			}
		}

		if (house.ruined)
		{
			// 내려앉은 지붕. 뚫린 구멍 사이로 서까래가 드러난다.
			const float holeX = cx - width * 0.14f * (house.door == 0 ? 1.0f : static_cast<float>(house.door));
			const float holeY = roofBottom + roofHeight * 0.52f;
			r->DrawEllipse(holeX, holeY, width * 0.2f, roofHeight * 0.2f, LitTint(kWindowDark, shade, 0.0f), 0.15f);
			r->DrawSegment(holeX - width * 0.18f, holeY - 4.0f, holeX + width * 0.16f, holeY + 5.0f, 2.2f, timber);
			r->DrawSegment(holeX - width * 0.10f, holeY + 6.0f, holeX + width * 0.20f, holeY - 3.0f, 1.8f, timber);
		}
	}

	// 나무 한 그루. (x, y)는 밑동. 좌표로 크기와 흔들림 박자를 정해 숲이 똑같아 보이지 않게 한다.
	void DrawTree(Renderer* r, float x, float y, const World::Tree& tree,
	              float shade, float warmth, float time)
	{
		const unsigned int hash = PropHash(tree.tileX * 7 + 3, tree.tileY * 13 + 1);
		const float size = 0.85f + static_cast<float>(hash % 30u) / 100.0f;
		const float sway = std::sin(time * 0.9f + static_cast<float>(hash % 628u) * 0.01f) * 0.8f;

		const Color shadow = { 0.0f, 0.0f, 0.0f, 0.38f };
		r->DrawEllipse(x, y, 13.0f * size, 4.5f * size, shadow, 0.0f, 0.0f, 3.0f);

		if (tree.kind == 1)
		{
			// 침엽수 — 세 겹 삼각형
			r->DrawRoundRect(x, y + 5.0f, 4.5f, 10.0f, 1.5f, LitTint(kBark, shade, warmth));
			r->DrawTriangle(x + sway * 0.3f, y + 18.0f * size, 28.0f * size, 22.0f * size, LitTint(kPine, shade * 0.8f, warmth));
			r->DrawTriangle(x + sway * 0.6f, y + 30.0f * size, 22.0f * size, 20.0f * size, LitTint(kPine, shade * 0.95f, warmth));
			r->DrawTriangle(x + sway, y + 41.0f * size, 14.0f * size, 16.0f * size, LitTint(kPine, shade * 1.15f, warmth));
		}
		else if (tree.kind == 2)
		{
			// 말라 죽은 나무 — 잎 없이 가지만 남았다
			const Color wood = LitTint(kDeadWood, shade, warmth);
			const float flip = (hash & 1u) ? 1.0f : -1.0f;
			const float top = y + 34.0f * size;
			r->DrawSegment(x, y + 1.0f, x + sway * 0.5f, top, 4.0f, wood);
			r->DrawSegment(x + sway * 0.2f, y + 16.0f * size, x + flip * 11.0f * size, y + 27.0f * size, 2.4f, wood);
			r->DrawSegment(x + sway * 0.35f, y + 23.0f * size, x - flip * 9.0f * size, y + 33.0f * size, 2.0f, wood);
			r->DrawSegment(x + sway * 0.5f, top - 2.0f, x + flip * 5.0f * size + sway, top + 7.0f * size, 1.6f, wood);
		}
		else
		{
			// 활엽수 — 뭉게뭉게 겹친 둥근 잎. 다섯에 하나는 병들어 누렇다
			const bool withered = (hash % 5u) == 0u;
			const float* leaf = withered ? kLeafDry : kLeaf;
			const float* leafLight = withered ? kLeafDry : kLeafLight;

			r->DrawRoundRect(x, y + 8.0f, 6.0f, 16.0f, 2.0f, LitTint(kBark, shade, warmth));
			r->DrawCircle(x + sway * 0.5f, y + 30.0f * size, 12.0f * size, LitTint(leaf, shade * 0.85f, warmth));
			r->DrawCircle(x - 8.0f * size + sway * 0.7f, y + 23.0f * size, 10.0f * size, LitTint(leaf, shade, warmth));
			r->DrawCircle(x + 8.0f * size + sway * 0.7f, y + 24.0f * size, 10.0f * size, LitTint(leaf, shade, warmth));
			r->DrawCircle(x - 2.5f * size + sway, y + 32.0f * size, 7.0f * size, LitTint(leafLight, shade * 1.1f, warmth));
		}
	}

	//
	// 울타리 한 칸.
	//
	// 첫 마을은 돌 성벽, 나머지 마을은 나무 말뚝 울타리다. 화면이 남쪽에서
	// 비스듬히 내려다보므로, 동서로 뻗은 북·남쪽 울타리는 말뚝이 옆으로
	// 늘어서 보이고, 남북으로 뻗은 서·동쪽 울타리는 말뚝이 앞뒤로 겹쳐 보인다.
	// 겹친 것은 먼 것(위)부터 그린다.
	//
	// 문 옆 기둥은 높고 굵다. 되찾은 마을이면 기둥에 등(나무 울타리)이나
	// 화로(성벽)가 켜져, 멀리서도 들어갈 문이 보인다.
	//
	void DrawWall(Renderer* r, float x, float y, const World::Wall& wall,
	              float shade, float warmth, bool lit, float time)
	{
		const unsigned int hash = PropHash(wall.tileX * 5 + 1, wall.tileY * 3 + 2);
		const bool alongX = (wall.side == World::kSideNorth || wall.side == World::kSideSouth);

		auto lamp = [&](float lampX, float lampY)
		{
			if (!lit)
			{
				return;
			}
			const float flicker = 0.85f + 0.15f * std::sin(time * 9.0f + static_cast<float>(hash % 97u));
			const Color glow = { kFlame[0], kFlame[1], kFlame[2], 0.9f };
			r->DrawCircle(lampX, lampY, 3.2f, glow, 2.4f * flicker);
		};

		if (wall.stone)
		{
			const Color stone = LitTint(kStone, shade, warmth);
			const Color dark = LitTint(kStoneDark, shade, warmth);
			const Color top = LitTint(kStone, shade * 1.15f, warmth);

			if (wall.post)
			{
				// 문 기둥 — 위에 화로
				r->DrawRoundRect(x, y + 23.0f, 16.0f, 46.0f, 2.0f, stone);
				r->DrawRoundRect(x, y + 47.0f, 20.0f, 5.0f, 1.0f, top);
				r->DrawRoundRect(x, y + 12.0f, 16.0f, 1.2f, 0.0f, dark);
				r->DrawRoundRect(x, y + 28.0f, 16.0f, 1.2f, 0.0f, dark);
				lamp(x, y + 53.0f);
				return;
			}
			if (wall.corner)
			{
				// 모서리 망루
				r->DrawRoundRect(x, y + 19.0f, 36.0f, 38.0f, 2.0f, stone);
				r->DrawRoundRect(x, y + 36.0f, 36.0f, 2.5f, 0.0f, top);
				for (int i = -1; i <= 1; ++i)
				{
					r->DrawRoundRect(x + i * 12.0f, y + 41.0f, 8.0f, 7.0f, 1.0f, stone);
				}
				r->DrawRoundRect(x, y + 12.0f, 36.0f, 1.2f, 0.0f, dark);
				r->DrawRoundRect(x, y + 25.0f, 36.0f, 1.2f, 0.0f, dark);
				return;
			}
			if (alongX)
			{
				// 가로로 뻗은 성벽 — 돌 두 켜와 그 위 성가퀴
				r->DrawRoundRect(x, y + 13.0f, 32.0f, 26.0f, 1.5f, stone);
				r->DrawRoundRect(x, y + 13.0f, 32.0f, 1.2f, 0.0f, dark);
				const float joint = ((wall.tileX & 1) == 0) ? -6.0f : 6.0f;   // 줄눈을 엇갈리게
				r->DrawRoundRect(x + joint, y + 6.5f, 1.2f, 12.0f, 0.0f, dark);
				r->DrawRoundRect(x - joint, y + 19.5f, 1.2f, 12.0f, 0.0f, dark);
				r->DrawRoundRect(x, y + 25.5f, 32.0f, 2.0f, 0.0f, top);
				r->DrawRoundRect(x - 8.0f, y + 29.5f, 10.0f, 6.0f, 1.0f, stone);
				r->DrawRoundRect(x + 8.0f, y + 29.5f, 10.0f, 6.0f, 1.0f, stone);
				return;
			}
			// 세로로 뻗은 성벽 — 앞뒤로 두 덩이
			for (int i = 0; i < 2; ++i)
			{
				const float groundY = y + 8.0f - 16.0f * static_cast<float>(i);   // 먼 것(위)부터
				r->DrawRoundRect(x, groundY + 14.0f, 18.0f, 28.0f, 1.5f, i == 0 ? dark : stone);
				r->DrawRoundRect(x, groundY + 30.5f, 12.0f, 6.0f, 1.0f, stone);
				r->DrawRoundRect(x, groundY + 14.0f, 18.0f, 1.2f, 0.0f, dark);
			}
			return;
		}

		// ── 나무 말뚝 울타리 ──
		const Color wood = LitTint(kWood, shade, warmth);
		const Color dark = LitTint(kWoodDark, shade, warmth);

		if (wall.post)
		{
			// 문 기둥 — 위에 등
			r->DrawRoundRect(x, y + 22.0f, 10.0f, 44.0f, 2.0f, dark);
			r->DrawTriangle(x, y + 47.0f, 10.0f, 7.0f, dark);
			r->DrawRoundRect(x, y + 34.0f, 14.0f, 3.0f, 1.0f, wood);   // 등을 거는 가로대
			lamp(x + 5.0f, y + 30.0f);
			return;
		}
		if (wall.corner)
		{
			r->DrawRoundRect(x, y + 18.0f, 11.0f, 36.0f, 2.5f, dark);
			r->DrawTriangle(x, y + 39.0f, 11.0f, 7.0f, dark);
			return;
		}
		if (alongX)
		{
			// 가로 울타리 — 가로대 두 줄 위로 말뚝 넷
			r->DrawRoundRect(x, y + 9.0f, 32.0f, 3.0f, 1.0f, dark);
			r->DrawRoundRect(x, y + 21.0f, 32.0f, 3.0f, 1.0f, dark);
			for (int i = 0; i < 4; ++i)
			{
				const float stakeX = x - 12.0f + 8.0f * static_cast<float>(i);
				const float height = 25.0f + static_cast<float>((hash >> (i * 3)) & 7u) * 0.8f;
				r->DrawRoundRect(stakeX, y + height * 0.5f, 6.0f, height, 1.5f, wood);
				r->DrawTriangle(stakeX, y + height + 2.5f, 6.0f, 5.0f, wood);
			}
			return;
		}
		// 세로 울타리 — 말뚝이 앞뒤로 겹쳐 선다
		for (int i = 0; i < 2; ++i)
		{
			const float groundY = y + 8.0f - 16.0f * static_cast<float>(i);   // 먼 것(위)부터
			const float height = 25.0f + static_cast<float>((hash >> (i * 4)) & 7u) * 0.8f;
			const Color& stake = (i == 0) ? dark : wood;
			r->DrawRoundRect(x, groundY + height * 0.5f, 7.0f, height, 1.5f, stake);
			r->DrawTriangle(x, groundY + height + 2.5f, 7.0f, 5.0f, stake);
		}
	}

	void DrawTorch(Renderer* r, float x, float y, float time, float seed)
	{
		// 땅에 꽂은 장대, 쇠 받침, 그 위에서 일렁이는 불
		const float flicker = 0.8f + 0.2f * std::sin(time * 11.0f + seed * 3.1f)
		                    + 0.1f * std::sin(time * 23.0f + seed);
		DrawShadow(r, x, y, 5.0f, 0.3f);
		r->DrawRoundRect(x, y + 15.0f, 4.0f, 30.0f, 1.5f, Tint(kWoodDark));
		r->DrawRoundRect(x, y + 31.0f, 9.0f, 4.0f, 1.5f, Tint(kMetal, 0.9f));
		r->DrawTriangle(x, y + 39.0f, 9.0f, 14.0f * flicker, Tint(kEmber), 0.0f, 1.6f * flicker);
		r->DrawTriangle(x, y + 37.5f, 5.0f, 9.0f * flicker, Tint(kFlame), 0.0f, 2.6f * flicker);
	}

	//
	// 마을 사람이 제 집 둘레에 놓는 것. 돈이 모일 때마다 하나씩 는다.
	// 간판과 깃발은 그 사람의 빛깔이라, 누구네 구역인지 멀리서도 알 수 있다.
	//
	void DrawDecor(Renderer* r, float x, float y, Decor kind, const float tint[3],
	               float shade, float warmth, float time, unsigned int seed)
	{
		const Color wood = LitTint(kWood, shade, warmth);
		const Color dark = LitTint(kWoodDark, shade, warmth);
		const Color own = LitTint(tint, shade, warmth);

		switch (kind)
		{
		case Decor::Sign:
		{
			DrawShadow(r, x, y, 9.0f, 0.3f);
			r->DrawRoundRect(x - 8.0f, y + 14.0f, 3.0f, 28.0f, 1.0f, dark);
			r->DrawRoundRect(x + 8.0f, y + 14.0f, 3.0f, 28.0f, 1.0f, dark);
			r->DrawRoundRect(x, y + 26.0f, 24.0f, 13.0f, 2.0f, own);
			r->DrawRoundRect(x, y + 26.0f, 16.0f, 2.0f, 0.5f, dark);
			r->DrawRoundRect(x, y + 21.5f, 10.0f, 1.6f, 0.5f, dark);
			break;
		}

		case Decor::Lantern:
		{
			const float flicker = 0.85f + 0.15f * std::sin(time * 8.0f + static_cast<float>(seed % 97u));
			DrawShadow(r, x, y, 5.0f, 0.3f);
			r->DrawRoundRect(x, y + 20.0f, 4.0f, 40.0f, 1.5f, dark);
			r->DrawRoundRect(x + 5.0f, y + 38.0f, 11.0f, 2.0f, 0.5f, dark);
			r->DrawRoundRect(x + 9.0f, y + 32.0f, 7.0f, 9.0f, 2.0f, Tint(kBronze, shade));
			const Color glow = { kFlame[0], kFlame[1], kFlame[2], 0.95f };
			r->DrawCircle(x + 9.0f, y + 32.0f, 2.8f, glow, 2.2f * flicker);
			break;
		}

		case Decor::Planter:
		{
			DrawShadow(r, x, y, 8.0f, 0.3f);
			r->DrawRoundRect(x, y + 5.0f, 15.0f, 10.0f, 3.0f, LitTint(kRust, shade, warmth));
			const Color leaf = LitTint(kLeaf, shade, warmth);
			r->DrawCircle(x - 4.0f, y + 13.0f, 4.5f, leaf);
			r->DrawCircle(x + 4.0f, y + 13.5f, 4.5f, leaf);
			r->DrawCircle(x, y + 16.0f, 4.5f, leaf);
			r->DrawCircle(x - 3.0f, y + 17.0f, 1.8f, own);
			r->DrawCircle(x + 3.5f, y + 15.5f, 1.8f, own);
			r->DrawCircle(x + 0.5f, y + 19.5f, 1.8f, own);
			break;
		}

		case Decor::Banner:
		{
			const float wave = std::sin(time * 2.4f + static_cast<float>(seed % 61u)) * 2.0f;
			DrawShadow(r, x, y, 4.0f, 0.3f);
			r->DrawSegment(x, y, x, y + 46.0f, 2.2f, dark);
			r->DrawRoundRect(x + 8.0f + wave * 0.3f, y + 37.0f, 14.0f, 17.0f, 1.0f, own, wave * 0.02f);
			r->DrawTriangle(x + 8.0f + wave * 0.5f, y + 26.5f, 14.0f, 6.0f, own, 3.14159265f);
			break;
		}

		case Decor::Bench:
		{
			DrawShadow(r, x, y, 14.0f, 0.3f);
			r->DrawRoundRect(x - 10.0f, y + 3.5f, 3.0f, 7.0f, 0.5f, dark);
			r->DrawRoundRect(x + 10.0f, y + 3.5f, 3.0f, 7.0f, 0.5f, dark);
			r->DrawRoundRect(x, y + 8.0f, 27.0f, 4.0f, 1.0f, wood);
			r->DrawRoundRect(x, y + 15.0f, 27.0f, 3.0f, 1.0f, wood);
			break;
		}

		case Decor::Crates:
		default:
		{
			DrawShadow(r, x, y, 13.0f, 0.3f);
			r->DrawRoundRect(x - 6.0f, y + 6.5f, 13.0f, 13.0f, 1.5f, wood);
			r->DrawRoundRect(x + 7.0f, y + 6.0f, 12.0f, 12.0f, 1.5f, LitTint(kWood, shade * 0.9f, warmth));
			r->DrawRoundRect(x + 0.5f, y + 17.5f, 12.0f, 11.0f, 1.5f, LitTint(kWood, shade * 1.08f, warmth));
			r->DrawRoundRect(x - 6.0f, y + 6.5f, 13.0f, 1.4f, 0.0f, dark);
			r->DrawRoundRect(x + 0.5f, y + 17.5f, 12.0f, 1.4f, 0.0f, dark);
			break;
		}
		}
	}

	void DrawCanopy(Renderer* r, float x, float y, int tileX, int tileY, float shade, float warmth)
	{
		// 지나갈 수 없는 숲 한 칸. 둥근 덩어리로 그렸더니 그냥 어두운 땅처럼 보여
		// "아무것도 없는데 못 간다"가 되었다. 뾰족한 침엽수 두 겹으로 그려
		// 한눈에 나무가 빽빽한 곳으로 읽히게 한다. 칸마다 자리와 키를 흔들어
		// 이웃 칸과 겹치면 줄 맞춰 심은 것이 아니라 우거진 숲이 된다.
		const unsigned int hash = PropHash(tileX * 3 + 11, tileY * 5 + 7);
		const float jitterX = (static_cast<float>(hash & 0xFFu) / 255.0f - 0.5f) * 12.0f;
		const float jitterY = (static_cast<float>((hash >> 8) & 0xFFu) / 255.0f - 0.5f) * 8.0f;
		const float size = 0.85f + static_cast<float>((hash >> 16) & 0xFu) / 15.0f * 0.35f;
		const float tone = 0.8f + static_cast<float>((hash >> 20) & 0xFu) / 15.0f * 0.35f;

		const float baseX = x + jitterX;
		const float baseY = y + jitterY - 8.0f;   // 밑동은 칸 아래쪽. 위로 솟는다

		r->DrawTriangle(baseX, baseY + 13.0f * size, 30.0f * size, 26.0f * size,
		                LitTint(kCanopy, shade * tone, warmth));
		r->DrawTriangle(baseX, baseY + 28.0f * size, 21.0f * size, 20.0f * size,
		                LitTint(kCanopyLight, shade * tone, warmth));
	}

	// 미니맵 표식. 종류마다 모양과 색을 다르게 해 한눈에 구분되게 한다.
	//   종탑 = 지붕 달린 탑(침묵은 잿빛, 울린 것은 불빛)   굳은 사람 = 마름모
	//   동행자 = 둥근 얼굴   화톳불 = 불꽃   보급품 = 원   나무통 = 둥근 막대   잔해 = 작은 네모
	void DrawMapMarker(Renderer* r, int marker, float x, float y, float warmth)
	{
		const Color outline = { 0.01f, 0.01f, 0.02f, 0.9f };

		switch (marker)
		{
		case kMarkTowerSilent:
		case kMarkTowerRung:
		{
			const bool rung = marker == kMarkTowerRung;
			const Color body = rung ? Tint(kFlame) : Tint(kMapTower);
			if (rung)
			{
				const Color halo = { kFlame[0], kFlame[1], kFlame[2], 0.22f };
				r->DrawCircle(x, y + 1.0f, 8.5f, halo);
			}
			r->DrawRoundRect(x, y - 1.5f, 7.0f, 7.0f, 1.0f, outline);
			r->DrawTriangle(x, y + 4.0f, 11.0f, 7.0f, outline);
			r->DrawRoundRect(x, y - 1.5f, 5.0f, 5.0f, 0.8f, body);
			r->DrawTriangle(x, y + 3.8f, 8.5f, 5.0f, body);
			break;
		}

		case kMarkSleeper:
		{
			const Color body = Blend(kMapFrozen, kLiving, warmth);
			r->DrawRoundRect(x, y, 7.5f, 7.5f, 1.0f, outline, 0.7854f);
			r->DrawRoundRect(x, y, 5.2f, 5.2f, 0.6f, body, 0.7854f);
			break;
		}

		case kMarkCompanion:
			r->DrawCircle(x, y, 4.2f, outline);
			r->DrawCircle(x, y, 3.0f, Tint(kLiving));
			break;

		case kMarkFire:
			r->DrawTriangle(x, y + 1.0f, 8.0f, 9.0f, outline);
			r->DrawTriangle(x, y + 0.8f, 6.0f, 7.0f, Tint(kEmber));
			r->DrawCircle(x, y - 1.0f, 1.3f, Tint(kFlameCore));
			break;

		case kMarkSupply:
			r->DrawCircle(x, y, 3.6f, outline);
			r->DrawCircle(x, y, 2.6f, Tint(kMapSupply));
			break;

		case kMarkBarrel:
			r->DrawRoundRect(x, y, 5.0f, 6.4f, 2.0f, outline);
			r->DrawRoundRect(x, y, 3.4f, 4.8f, 1.4f, Tint(kMapBarrel));
			break;

		case kMarkWarden:
			r->DrawCircle(x, y, 6.5f, outline);
			r->DrawRoundRect(x, y, 7.4f, 7.4f, 1.0f, Tint(kEyeRed, 0.9f), 0.7854f);
			r->DrawCircle(x, y, 1.6f, Tint(kFlameCore));
			break;

		case kMarkEnemy:
			r->DrawCircle(x, y, 2.8f, outline);
			r->DrawCircle(x, y, 1.9f, Tint(kEyeRed));
			break;

		case kMarkParty:
		{
			const Color green = { 0.46f, 0.86f, 0.56f, 1.0f };
			r->DrawRoundRect(x, y, 6.6f, 6.6f, 1.6f, outline, 0.7854f);
			r->DrawRoundRect(x, y, 4.6f, 4.6f, 1.0f, green, 0.7854f);
			break;
		}

		default:   // kMarkRubble
			r->DrawRoundRect(x, y, 5.4f, 5.4f, 0.6f, outline, 0.3f);
			r->DrawRoundRect(x, y, 3.8f, 3.8f, 0.4f, Tint(kMapRubble), 0.3f);
			break;
		}
	}

	void DrawItemIcon(Renderer* r, ItemKind kind, float x, float y, float size, float alpha)
	{
		const float s = size;
		switch (kind)
		{
		case ItemKind::HealPack:
		{
			// 붉은 병 위의 흰 십자
			const Color glass = Tint(kHealRed, 1.1f, alpha);
			const Color white = { 0.95f, 0.93f, 0.90f, alpha };
			r->DrawRoundRect(x, y + s * 0.30f, s * 0.20f, s * 0.16f, s * 0.04f, Tint(kWood, 1.2f, alpha));
			r->DrawRoundRect(x, y + s * 0.20f, s * 0.16f, s * 0.12f, s * 0.03f, glass);
			r->DrawCircle(x, y - s * 0.08f, s * 0.32f, glass);
			r->DrawRoundRect(x, y - s * 0.08f, s * 0.30f, s * 0.09f, s * 0.04f, white);
			r->DrawRoundRect(x, y - s * 0.08f, s * 0.09f, s * 0.30f, s * 0.04f, white);
			break;
		}

		case ItemKind::Torch:
		{
			// 비스듬한 자루 끝에 불꽃
			r->DrawSegment(x - s * 0.16f, y - s * 0.38f, x + s * 0.06f, y + s * 0.14f, s * 0.11f, Tint(kWoodDark, 1.3f, alpha));
			r->DrawRoundRect(x + s * 0.06f, y + s * 0.12f, s * 0.18f, s * 0.08f, s * 0.03f, Tint(kMetal, 1.0f, alpha), -0.4f);
			r->DrawTriangle(x + s * 0.09f, y + s * 0.30f, s * 0.30f, s * 0.36f, Tint(kEmber, 1.0f, alpha), -0.35f);
			r->DrawTriangle(x + s * 0.08f, y + s * 0.27f, s * 0.16f, s * 0.22f, Tint(kFlameCore, 1.0f, alpha), -0.35f);
			break;
		}

		case ItemKind::Oil:
		{
			// 호박빛 기름이 찰랑이는 병과 코르크
			const Color glass = { 0.62f, 0.66f, 0.70f, 0.55f * alpha };
			const Color oil = { 0.92f, 0.62f, 0.18f, alpha };
			r->DrawRoundRect(x, y - s * 0.06f, s * 0.44f, s * 0.52f, s * 0.12f, glass);
			r->DrawRoundRect(x, y - s * 0.14f, s * 0.38f, s * 0.32f, s * 0.10f, oil);
			r->DrawRoundRect(x, y + s * 0.26f, s * 0.16f, s * 0.14f, s * 0.03f, glass);
			r->DrawRoundRect(x, y + s * 0.36f, s * 0.18f, s * 0.08f, s * 0.03f, Tint(kWood, 1.3f, alpha));
			r->DrawRoundRect(x - s * 0.10f, y - s * 0.02f, s * 0.05f, s * 0.20f, s * 0.02f, Color{ 1.0f, 0.95f, 0.8f, 0.6f * alpha });
			break;
		}

		case ItemKind::WeaponStone:
		{
			// 금빛 무리 속의 짧은 칼(떨어진 강화석과 같은 그림)
			const Color halo = { kGold[0], kGold[1], kGold[2], 0.22f * alpha };
			r->DrawCircle(x, y, s * 0.42f, halo, 0.0f, s * 0.12f);
			r->DrawSegment(x - s * 0.16f, y - s * 0.20f, x + s * 0.22f, y + s * 0.24f, s * 0.11f, Tint(kSteel, 1.1f, alpha));
			r->DrawSegment(x - s * 0.25f, y - s * 0.02f, x - s * 0.02f, y - s * 0.27f, s * 0.09f, Tint(kGold, 1.0f, alpha));
			r->DrawSegment(x - s * 0.16f, y - s * 0.20f, x - s * 0.28f, y - s * 0.32f, s * 0.10f, Tint(kWood, 1.1f, alpha));
			break;
		}

		default:
			break;
		}
	}

	// 종탑 한 채. ringing이면 종이 흔들리고 머리 위로 눈금이 찬다.
	void DrawTower(Renderer* r, float x, float y, const Regions::Region& region,
	               float ringProgress, bool ringing, float time)
	{
		// 아직 침묵한 종탑도 연기 너머로는 보인다. 무한 맵에서 갈 곳이 된다.
		// 울린 종탑은 돌이 종의 불빛을 받아 따뜻해진다.
		const float shade = region.rung ? 1.0f : 0.55f;
		const float warm = region.rung ? 0.22f : 0.0f;
		const Color stone = Blend(kStone, kFlame, warm, shade);
		const Color stoneDark = Blend(kStoneDark, kFlame, warm * 0.6f, shade);
		const Color groove = { stoneDark.r, stoneDark.g, stoneDark.b, 0.5f };
		const Color side = { stoneDark.r, stoneDark.g, stoneDark.b, 0.55f };

		DrawShadow(r, x, y - 1.0f, 26.0f, 0.45f);

		// 기단과 몸체. 오른쪽에 그늘을 한 줄 넣어 두께를 흉내낸다.
		r->DrawRoundRect(x, y + 4.0f, 38.0f, 9.0f, 2.0f, stoneDark);
		r->DrawRoundRect(x, y + 31.0f, 28.0f, 46.0f, 1.5f, stone);
		r->DrawRoundRect(x + 9.5f, y + 31.0f, 9.0f, 46.0f, 1.0f, side);
		for (int i = 0; i < 3; ++i)
		{
			r->DrawRoundRect(x, y + 25.0f + i * 9.0f, 28.0f, 1.0f, 0.5f, groove);   // 줄눈
		}

		// 아치형 문
		r->DrawRoundRect(x - 2.0f, y + 15.5f, 9.0f, 15.0f, 4.5f, Tint(kCrow));

		// 종루 — 뚫린 창 안에 종이 걸려 있다. 울린 종루는 안이 불빛으로 차 있다.
		r->DrawRoundRect(x, y + 47.0f, 17.0f, 15.0f, 7.5f,
		                        region.rung ? Blend(kWoodDark, kFlame, 0.35f) : Tint(kCrow));

		// 종. 당기는 중이면 당긴 만큼 크게 흔들린다.
		const float swing = ringing ? std::sin(time * 9.0f) * 0.35f * ringProgress : 0.0f;
		const float bellX = x + std::sin(swing) * 3.0f;
		const float bellGlow = region.rung ? kGlowBell : 0.0f;
		const Color bell = region.rung ? Blend(kBronze, kFlame, 0.45f) : Tint(kBronze, shade * 0.6f);
		r->DrawEllipse(bellX, y + 47.5f, 4.8f, 5.2f, bell, swing, bellGlow);
		r->DrawRoundRect(bellX + std::sin(swing), y + 43.0f, 11.0f, 2.4f, 1.0f, bell, swing, bellGlow);

		// 처마, 지붕, 꼭대기 장식
		r->DrawRoundRect(x, y + 56.0f, 34.0f, 4.0f, 1.0f, stoneDark);
		r->DrawTriangle(x, y + 68.5f, 37.0f, 22.0f, Tint(kRoof, shade));
		r->DrawRoundRect(x, y + 83.0f, 2.2f, 9.0f, 1.0f, stoneDark);
		r->DrawRoundRect(x, y + 84.5f, 7.0f, 2.2f, 1.0f, stoneDark);

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
				DrawCrow(r, x + perchX[i], y + perchY, facing[i], time, seed + i * 1.7f);
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
			DrawFigure(r, x + 27.0f, y - 3.0f, keeper, &handX, &handY);
			r->DrawSegment(bellX, y + 42.0f, handX, handY, 1.2f, Tint(kRope));
		}

		// 종을 당기는 중이면 종탑 위로 불씨 눈금이 찬다.
		if (ringing)
		{
			const int pips = 8;
			const int filled = static_cast<int>(ringProgress * pips + 0.5f);
			for (int i = 0; i < pips; ++i)
			{
				const bool lit = i < filled;
				r->DrawCircle(x - 28.0f + i * 8.0f, y + 98.0f, lit ? 3.0f : 2.2f,
				                     lit ? Tint(kFlame) : Tint(kStoneDark, 0.8f),
				                     lit ? kGlowGauge : 0.0f);
			}
		}
	}

	// 적 한 마리. 어둠 속에서는 눈빛이 먼저 보인다.
	void DrawEnemy(Renderer* r, float x, float y, const Enemy& enemy, float facing, float time)
	{
		// 쓰러지면 연기처럼 흩어진다
		const float fade = (enemy.dying > 0.0f) ? std::max(0.0f, 1.0f - enemy.dying / 0.45f) : 1.0f;
		if (fade <= 0.0f)
		{
			return;
		}

		const float flash = (enemy.hitFlash > 0.0f) ? 0.85f : 0.0f;
		const float bob = std::sin(time * 3.0f + enemy.phase) * 1.5f;

		// 맞은 순간에는 하얗게 번쩍인다
		auto body = [flash, fade](const float c[3], float scale)
		{
			Color color = { Mix(c[0] * scale, 1.0f, flash), Mix(c[1] * scale, 1.0f, flash),
			                Mix(c[2] * scale, 1.0f, flash), fade };
			return color;
		};

		const Color shadow = { 0.0f, 0.0f, 0.0f, 0.35f * fade };

		switch (enemy.kind)
		{
		case EnemyKind::Wraith:
		{
			// 떠도는 그림자 — 발 대신 연기 꼬리로 떠다닌다
			const float sway = std::sin(time * 4.0f + enemy.phase) * 2.0f;
			r->DrawEllipse(x, y, 10.0f, 3.5f, shadow, 0.0f, 0.0f, 2.5f);
			r->DrawTriangle(x + sway, y + 10.0f + bob, 20.0f, 18.0f, body(kShadeBody, 1.0f), 3.14159f);
			r->DrawEllipse(x, y + 21.0f + bob, 11.0f, 12.0f, body(kShadeBody, 1.0f));
			r->DrawEllipse(x - 9.0f, y + 15.0f + bob * 1.3f, 3.5f, 6.5f, body(kShadeBody, 1.2f), 0.5f + sway * 0.05f);
			r->DrawEllipse(x + 9.0f, y + 15.0f - bob * 1.3f, 3.5f, 6.5f, body(kShadeBody, 1.2f), -0.5f + sway * 0.05f);

			const Color eye = { kEyePale[0], kEyePale[1], kEyePale[2], fade };
			r->DrawCircle(x - 3.5f + facing * 1.5f, y + 23.0f + bob, 1.8f, eye, kGlowEye * fade);
			r->DrawCircle(x + 3.5f + facing * 1.5f, y + 23.0f + bob, 1.8f, eye, kGlowEye * fade);
			break;
		}

		case EnemyKind::Husk:
		{
			// 삼켜진 자 — 사람의 모습 그대로 검게 굳어 웅크린 채 걸어온다
			FigureLook look;
			look.body = body(kShadeBody, 1.0f);
			look.skin = body(kShadeSkin, 1.0f);
			look.hair = body(kShadeBody, 0.7f);
			look.hooded = false;
			look.posture = 0.3f + 0.08f * std::sin(time * 2.0f + enemy.phase);
			look.facing = facing;
			look.bob = std::fabs(bob);
			DrawFigure(r, x, y, look);

			// DrawFigure의 머리 위치와 같은 계산
			const float height = Mix(0.70f, 1.0f, look.posture);
			const float hunch = 1.0f - look.posture;
			const float headX = x + facing * hunch * 5.5f;
			const float headY = y + (26.0f - hunch * 3.0f) * height + look.bob;
			const Color eye = { kEyeRed[0], kEyeRed[1], kEyeRed[2], fade };
			r->DrawCircle(headX + facing * 0.4f, headY + 0.5f, 1.2f, eye, kGlowEye * fade);
			r->DrawCircle(headX + facing * 3.4f, headY + 0.5f, 1.2f, eye, kGlowEye * fade);
			break;
		}

		case EnemyKind::Warden:
		{
			// 종탑지기 — 녹슨 종을 가슴에 매단 거대한 두건의 그림자. 까마귀가 곁을 맴돈다
			const float engaged = enemy.engaged ? 1.0f : 0.0f;
			const Color aura = { 0.06f, 0.02f, 0.09f, (0.28f + 0.18f * engaged) * fade };

			r->DrawEllipse(x, y, 30.0f, 9.0f, shadow, 0.0f, 0.0f, 4.0f);
			r->DrawCircle(x, y + 40.0f, 46.0f, aura, 0.0f, 16.0f);

			r->DrawTriangle(x, y + 30.0f + bob * 0.5f, 60.0f, 60.0f, body(kWardenRobe, 1.0f));
			r->DrawRoundRect(x, y + 51.0f + bob, 48.0f, 17.0f, 8.0f, body(kWardenRobe, 1.2f));

			// 몸을 감은 쇠사슬
			const Color chain = body(kRust, 0.8f);
			r->DrawSegment(x - 21.0f, y + 49.0f + bob, x + 15.0f, y + 22.0f + bob, 2.2f, chain);
			r->DrawSegment(x + 21.0f, y + 49.0f + bob, x - 15.0f, y + 20.0f + bob, 2.2f, chain);

			// 가슴의 녹슨 종
			r->DrawEllipse(x, y + 35.0f + bob, 8.0f, 9.0f, body(kRust, 1.0f));
			r->DrawRoundRect(x, y + 27.5f + bob, 19.0f, 3.0f, 1.5f, body(kRust, 0.8f));

			// 두건과 텅 빈 얼굴, 그 안의 붉은 눈
			r->DrawTriangle(x, y + 82.0f + bob, 18.0f, 16.0f, body(kWardenRobe, 1.1f));
			r->DrawEllipse(x, y + 67.0f + bob, 15.5f, 15.0f, body(kWardenRobe, 1.1f));
			const Color hollow = { 0.0f, 0.0f, 0.0f, fade };
			r->DrawEllipse(x + facing * 1.5f, y + 65.0f + bob, 9.0f, 8.0f, hollow);

			const Color eye = { kEyeRed[0], kEyeRed[1], kEyeRed[2], fade };
			const float eyeGlow = kGlowEye * (1.0f + engaged) * fade;
			r->DrawCircle(x + facing * 1.5f - 3.8f, y + 66.0f + bob, 2.2f, eye, eyeGlow);
			r->DrawCircle(x + facing * 1.5f + 3.8f, y + 66.0f + bob, 2.2f, eye, eyeGlow);

			for (int k = 0; k < 2; ++k)
			{
				const float angle = time * 1.4f + enemy.phase + k * 3.14159f;
				DrawCrow(r, x + std::cos(angle) * 40.0f, y + 74.0f + std::sin(angle) * 10.0f,
				         std::sin(angle) > 0.0f ? -1.0f : 1.0f, time, enemy.phase + k);
			}
			break;
		}
		}

		// 상처 입은 적만 머리 위에 체력을 보인다. 종탑지기는 화면 위쪽의 큰 막대로 보인다.
		if (enemy.kind != EnemyKind::Warden && enemy.dying == 0.0f && enemy.health < enemy.maxHealth)
		{
			const float barY = y + (enemy.kind == EnemyKind::Husk ? 40.0f : 37.0f);
			const float ratio = std::max(0.0f, enemy.health / enemy.maxHealth);
			const Color back = { 0.0f, 0.0f, 0.0f, 0.7f };
			r->DrawRoundRect(x, barY, 26.0f, 4.5f, 2.0f, back);
			if (ratio > 0.0f)
			{
				r->DrawRoundRect(x - 12.0f * (1.0f - ratio), barY, 24.0f * ratio, 2.5f, 1.2f, Tint(kEyeRed, 0.9f));
			}
		}
	}

	// 떨어진 물건 하나. 종류마다 모양과 빛깔이 다르다.
	void DrawPickup(Renderer* r, float x, float y, const Pickup& pickup, float seed, float time)
	{
		// 오래 놓여 있던 것은 사라지기 전에 깜빡인다
		const float remaining = 90.0f - pickup.age;
		const float blink = (remaining < 5.0f && std::fmod(pickup.age * 6.0f, 1.0f) < 0.5f) ? 0.35f : 1.0f;
		const float bob = std::sin(time * 3.0f + seed * 0.05f) * 1.5f;
		const float cy = y + 7.0f + bob;

		const Color shadow = { 0.0f, 0.0f, 0.0f, 0.3f * blink };
		r->DrawEllipse(x, y, 5.5f, 2.0f, shadow, 0.0f, 0.0f, 1.5f);

		switch (pickup.kind)
		{
		case PickupKind::Soul:
		{
			// 영혼석 — 푸르게 빛나는 마름모
			const float size = 6.0f + std::min(4.0f, pickup.value * 0.5f);
			r->DrawRoundRect(x, cy, size, size, 1.0f, Tint(kSoulColor, 1.0f, blink), 0.7854f, kGlowSoul * blink);
			r->DrawRoundRect(x, cy, size * 0.4f, size * 0.4f, 0.5f, Tint(kFlameCore, 1.0f, blink), 0.7854f, kGlowSoul * blink);
			break;
		}

		case PickupKind::Heal:
		{
			// 회복약 — 붉은 병 위의 흰 십자
			const Color white = { 0.95f, 0.93f, 0.90f, blink };
			r->DrawCircle(x, cy, 6.5f, Tint(kHealRed, 1.0f, blink), 1.0f * blink);
			r->DrawRoundRect(x, cy, 7.0f, 2.4f, 1.0f, white);
			r->DrawRoundRect(x, cy, 2.4f, 7.0f, 1.0f, white);
			break;
		}

		case PickupKind::Weapon:
		{
			// 강화석 — 금빛 무리 속의 짧은 칼
			const Color halo = { kGold[0], kGold[1], kGold[2], 0.25f * blink };
			r->DrawCircle(x, cy, 10.0f, halo, 0.0f, 3.0f);
			r->DrawSegment(x - 3.5f, cy - 4.5f, x + 5.0f, cy + 5.0f, 2.4f, Tint(kSteel, 1.0f, blink), 1.5f * blink);
			r->DrawSegment(x - 5.5f, cy - 0.5f, x - 0.5f, cy - 6.0f, 2.0f, Tint(kGold, 1.0f, blink), 2.0f * blink);
			r->DrawSegment(x - 3.5f, cy - 4.5f, x - 6.0f, cy - 7.0f, 2.2f, Tint(kWood, 1.0f, blink));
			break;
		}

		case PickupKind::Magnet:
		{
			// 자석 — 붉은 말굽 모양
			const Color red = Tint(kMagnetRed, 1.0f, blink);
			const Color tip = Tint(kSteel, 1.0f, blink);
			r->DrawRoundRect(x, cy - 4.0f, 11.0f, 4.0f, 2.0f, red, 0.0f, 1.2f * blink);
			r->DrawSegment(x - 4.0f, cy - 4.0f, x - 4.0f, cy + 4.0f, 3.2f, red, 1.2f * blink);
			r->DrawSegment(x + 4.0f, cy - 4.0f, x + 4.0f, cy + 4.0f, 3.2f, red, 1.2f * blink);
			r->DrawRoundRect(x - 4.0f, cy + 5.2f, 3.6f, 2.4f, 0.6f, tip);
			r->DrawRoundRect(x + 4.0f, cy + 5.2f, 3.6f, 2.4f, 0.6f, tip);
			break;
		}

		case PickupKind::Gold:
		{
			// 금화 — 납작하게 기울어 반짝인다. 많을수록 조금 크다
			const float size = 4.6f + std::min(2.5f, pickup.value * 0.4f);
			const float spin = 0.55f + 0.45f * std::fabs(std::sin(time * 2.6f + seed * 0.1f));
			r->DrawEllipse(x, cy, size * spin, size, Tint(kGold, 1.0f, blink), 0.0f, 0.9f * blink);
			r->DrawEllipse(x, cy, size * spin * 0.55f, size * 0.55f, Tint(kGold, 0.7f, blink));
			break;
		}
		}
	}

	// 등불에서 날아간 불씨. 날아온 길에 꼬리가 남는다.
	void DrawEmber(Renderer* r, float x, float y, float velocityX, float velocityY)
	{
		const float speed = std::sqrt(velocityX * velocityX + velocityY * velocityY);
		const float backX = (speed > 0.0f) ? -velocityX / speed : 0.0f;
		const float backY = (speed > 0.0f) ? velocityY / speed : 0.0f;   // 렌더러는 y가 위쪽

		for (int k = 3; k >= 1; --k)
		{
			r->DrawCircle(x + backX * k * 5.0f, y + backY * k * 5.0f, 3.2f - k * 0.6f,
			                     Tint(kEmber, 1.0f, 0.55f - k * 0.12f), kGlowEmber * 0.35f);
		}
		r->DrawCircle(x, y, 3.6f, Tint(kFlame), kGlowEmber);
		r->DrawCircle(x, y, 1.6f, Tint(kFlameCore), kGlowEmber * 1.4f);
	
	}

	// 종탑지기가 흩뿌린 어둠 구슬. 검은 속을 보랏빛 테가 둘러 어둠 속에서도 보인다.
	void DrawOrb(Renderer* r, float x, float y, float seed, float time)
	{
		const float pulse = 0.8f + 0.2f * std::sin(time * 12.0f + seed * 0.1f);
		const Color rim = { kOrbRim[0], kOrbRim[1], kOrbRim[2], 0.4f };
		r->DrawCircle(x, y, 8.0f * pulse, rim, 1.4f, 3.0f);
		r->DrawCircle(x, y, 5.0f, Tint(kOrbCore));
		r->DrawCircle(x, y, 1.8f, Tint(kOrbRim), 2.5f);
	
	}

	// 굳어 있다 깨어나는 사람. warmth가 그대로 색과 자세가 된다.
	// standing이면(말을 거는 중이면) 생기와 상관없이 일어서서 마주 본다.
	void DrawSleeper(Renderer* r, float x, float y, float warmth, bool standing, float facing, float time)
	{
		FigureLook look;
		look.body = Blend(kFrozen, kTunic, warmth);
		look.skin = Blend(kFrozen, kLiving, warmth);
		look.hair = Blend(kFrozen, kHair, warmth);
		look.hooded = false;
		look.posture = standing ? 1.0f : warmth * 0.85f;
		look.facing = facing;
		look.bob = 0.0f;
		DrawFigure(r, x, y, look);

		// 생기가 다 차면 머리 위에 불씨가 떠오른다. 말을 걸 수 있다는 뜻이다.
		if (warmth >= 1.0f)
		{
			const float hover = y + 40.0f + std::sin(time * 3.0f) * 1.5f;
			r->DrawRoundRect(x, hover, 4.6f, 4.6f, 1.0f, Tint(kFlame), 0.7854f, kGlowSpark);
			r->DrawCircle(x, hover, 1.2f, Tint(kFlameCore), kGlowSpark * 1.5f);
		}
	}

	// 따라나선 동행자. 어둠에 잠식될수록(alive가 줄수록) 색이 빠지고 몸이 움츠러든다.
	void DrawCompanion(Renderer* r, float x, float y, float alive, float facing, bool walking, float time)
	{
		FigureLook look;
		look.body = Blend(kFrozen, kTunic, alive);
		look.skin = Blend(kFrozen, kLiving, alive);
		look.hair = Blend(kFrozen, kHair, alive);
		look.hooded = false;
		look.posture = Mix(0.35f, 1.0f, alive);
		look.facing = facing;
		look.bob = walking ? std::fabs(std::sin(time * 9.0f + 1.3f)) * 1.4f : 0.0f;
		DrawFigure(r, x, y, look);
	}

	// 등불을 든 플레이어. 기름이 마르면 등불이 꺼진다.
	void DrawPlayer(Renderer* r, float x, float y, float oil, float facing, bool moving, float time)
	{
		const float lanternGlow = (oil > 0.0f) ? 0.35f + 0.65f * oil : 0.0f;

		FigureLook look;
		look.body = Tint(kCloak);
		look.skin = Tint(kLiving);
		look.hair = Tint(kHood);
		look.hooded = true;
		look.posture = 1.0f;
		look.facing = facing;
		look.bob = moving ? std::fabs(std::sin(time * 10.0f)) * 1.6f : 0.0f;

		float handX = x;
		float handY = y;
		DrawFigure(r, x, y, look, &handX, &handY);

		// 걸으면 크게, 서 있으면 숨 쉬듯 조금 흔들린다.
		const float swing = moving ? std::sin(time * 10.0f) * 0.28f
		                           : std::sin(time * 1.6f) * 0.06f;
		DrawLantern(r, handX, handY, swing, lanternGlow);
	}
}
