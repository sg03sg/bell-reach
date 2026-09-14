#include "stdafx.h"
#include "Game.h"

#include "Dialogue.h"
#include "Input.h"
#include "Progression.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace
{
	const int   kTileSize   = World::kTilePixels;

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
	// 땅. 역병이 지나간 마을이라 풀빛도 흙빛도 가라앉아 있다.
	const float kGrassColor[3] = { 0.20f, 0.25f, 0.17f };
	const float kPathColor[3]  = { 0.36f, 0.29f, 0.21f };
	const float kPlazaColor[3] = { 0.33f, 0.33f, 0.35f };
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
	const float kGlowWindow  = 2.5f;    // 되찾은 마을의 집 창문. 사람이 돌아와 불을 켰다

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

	// ── 대화 ────────────────────────────────────────────────────
	const float kCharsPerSecond = 28.0f;   // 한 글자씩 적히는 속도

	// UTF-8에서 글자 수를 센다. 이어지는 바이트(10xxxxxx)는 세지 않는다.
	int CountCharacters(const std::string& text)
	{
		int count = 0;
		for (unsigned char byte : text)
		{
			if ((byte & 0xC0) != 0x80 && byte != '\n')
			{
				++count;
			}
		}
		return count;
	}

	std::vector<std::string> SplitLines(const std::string& text)
	{
		std::vector<std::string> lines;
		size_t start = 0;
		while (true)
		{
			const size_t end = text.find('\n', start);
			lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
			if (end == std::string::npos)
			{
				break;
			}
			start = end + 1;
		}
		return lines;
	}

	// 월드 좌표의 차이를 방위로 바꾼다. 월드는 y가 아래로 커지므로 북쪽이 -y다.
	const char* DirectionName(float dx, float dy)
	{
		static const char* const kNames[8] =
		{
			"동쪽", "북동쪽", "북쪽", "북서쪽", "서쪽", "남서쪽", "남쪽", "남동쪽"
		};
		const float angle = std::atan2(-dy, dx);
		int sector = static_cast<int>(std::floor(angle / (3.14159265f / 4.0f) + 0.5f));
		sector = ((sector % 8) + 8) % 8;
		return kNames[sector];
	}

	std::string ReplaceAll(std::string text, const std::string& from, const std::string& to)
	{
		size_t position = 0;
		while ((position = text.find(from, position)) != std::string::npos)
		{
			text.replace(position, from.size(), to);
			position += to.size();
		}
		return text;
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

	// 집
	const float kPlaster[3]    = { 0.47f, 0.41f, 0.33f };   // 회반죽 벽
	const float kTimber[3]     = { 0.22f, 0.15f, 0.10f };   // 목조 뼈대
	const float kRoofTile[3]   = { 0.36f, 0.20f, 0.15f };   // 붉은 기와
	const float kRoofSlate[3]  = { 0.24f, 0.25f, 0.29f };   // 청석 지붕
	const float kThatch[3]     = { 0.42f, 0.34f, 0.20f };   // 초가
	const float kWindowDark[3] = { 0.05f, 0.05f, 0.07f };

	// 나무
	const float kLeaf[3]       = { 0.17f, 0.25f, 0.15f };
	const float kLeafLight[3]  = { 0.25f, 0.34f, 0.20f };
	const float kLeafDry[3]    = { 0.33f, 0.27f, 0.16f };   // 병든 잎
	const float kPine[3]       = { 0.12f, 0.21f, 0.16f };
	const float kBark[3]       = { 0.24f, 0.17f, 0.11f };
	const float kDeadWood[3]   = { 0.30f, 0.27f, 0.24f };

	// 미니맵 — 어두운 화면 구석에서도 한눈에 구분되도록 실제 색보다 밝게 잡는다
	const float kMapGrass[3]   = { 0.20f, 0.27f, 0.17f };
	const float kMapPath[3]    = { 0.47f, 0.38f, 0.26f };
	const float kMapPlaza[3]   = { 0.46f, 0.46f, 0.48f };
	const float kMapHouse[3]   = { 0.62f, 0.34f, 0.24f };
	const float kMapTree[3]    = { 0.11f, 0.19f, 0.12f };
	const float kMapFrozen[3]  = { 0.62f, 0.66f, 0.76f };
	const float kMapTower[3]   = { 0.60f, 0.63f, 0.70f };
	const float kMapRubble[3]  = { 0.58f, 0.55f, 0.50f };
	const float kMapSupply[3]  = { 0.40f, 0.80f, 0.64f };
	const float kMapBarrel[3]  = { 0.72f, 0.50f, 0.28f };

	const int   kMapTiles = 96;              // 지도 이미지 한 변의 타일 수
	const float kMapPixelsPerTile = 2.0f;    // 지도에서 한 칸이 차지하는 화면 픽셀
	const float kMapViewSize = 184.0f;       // 화면에 보이는 지도 한 변(px)

	const int kDrawTower     = 0;
	const int kDrawSleeper   = 1;
	const int kDrawProp      = 2;
	const int kDrawCompanion = 3;
	const int kDrawFire      = 4;
	const int kDrawPlayer    = 5;
	const int kDrawHouse     = 6;
	const int kDrawTree      = 7;
	const int kDrawEnemy     = 8;

	// 전투
	const float kShadeBody[3]  = { 0.045f, 0.045f, 0.060f };   // 삼켜진 자들의 몸. 연기보다 조금 짙다
	const float kShadeSkin[3]  = { 0.075f, 0.075f, 0.095f };
	const float kWardenRobe[3] = { 0.040f, 0.035f, 0.052f };
	const float kRust[3]       = { 0.36f, 0.20f, 0.12f };
	const float kEyeRed[3]     = { 1.00f, 0.28f, 0.16f };
	const float kEyePale[3]    = { 0.70f, 0.82f, 0.95f };
	const float kSoulColor[3]  = { 0.55f, 0.78f, 1.00f };
	const float kHealRed[3]    = { 0.74f, 0.14f, 0.14f };
	const float kSteel[3]      = { 0.82f, 0.84f, 0.90f };
	const float kGold[3]       = { 0.95f, 0.72f, 0.28f };
	const float kMagnetRed[3]  = { 0.80f, 0.16f, 0.16f };
	const float kOrbCore[3]    = { 0.08f, 0.03f, 0.12f };
	const float kOrbRim[3]     = { 0.62f, 0.30f, 0.90f };

	const float kGlowEye    = 4.0f;    // 어둠 속에서는 눈빛만 먼저 보인다
	const float kGlowEmber  = 7.0f;    // 날아가는 불씨
	const float kGlowSoul   = 3.5f;

	// 미니맵 표식 종류
	const int kMarkTowerSilent = 0;
	const int kMarkTowerRung   = 1;
	const int kMarkSleeper     = 2;
	const int kMarkCompanion   = 3;
	const int kMarkFire        = 4;
	const int kMarkSupply      = 5;
	const int kMarkBarrel      = 6;
	const int kMarkRubble      = 7;
	const int kMarkWarden      = 8;
	const int kMarkEnemy       = 9;

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

		default:   // kMarkRubble
			r->DrawRoundRect(x, y, 5.4f, 5.4f, 0.6f, outline, 0.3f);
			r->DrawRoundRect(x, y, 3.8f, 3.8f, 0.4f, Tint(kMapRubble), 0.3f);
			break;
		}
	}
}

void Game::Initialize(int windowWidth, int windowHeight, unsigned int seed)
{
	m_WindowWidth = windowWidth;
	m_WindowHeight = windowHeight;

	// 씨앗 하나로 섬 전체가 정해진다. 실행할 때마다 다른 섬이 된다.
	m_World = World(seed);
	m_Regions = Regions(seed);
	m_Random.seed(seed);

	m_Light.Configure(kLightCell);

	// 첫 종탑에서 열네 걸음 떨어진 곳에서 시작한다.
	// 종탑이 있는 줄의 십자길 위라 어느 씨앗에서도 걸을 수 있는 칸이 보장되고,
	// 연기 너머로 종탑과 그 앞의 종탑지기가 보인다. 종탑지기가 덤벼드는 거리보다
	// 멀어서, 먼저 주변의 그림자를 쓰러뜨리며 강해질 여유가 있다.
	const Regions::Region first = m_Regions.At(0, 0);
	m_PlayerX = TileToWorld(first.towerTileX - 14);
	m_PlayerY = TileToWorld(first.towerTileY);
	m_StartX = m_PlayerX;
	m_StartY = m_PlayerY;

	m_Combat = PlayerCombat();
	m_Combat.health = Progression::MaxHealth(m_Combat.level);
	m_Combat.healthShown = m_Combat.health;

	m_CameraX = m_PlayerX;
	m_CameraY = m_PlayerY;

	m_PlayerPath.push_back({ m_PlayerX, m_PlayerY, 0.0f });
}

bool Game::IsBlockedAtWorld(float worldX, float worldY) const
{
	return m_World.IsBlocked(FloorToInt(worldX / kTileSize),
	                         FloorToInt(worldY / kTileSize));
}

float Game::StructureShade(float worldX, float worldY) const
{
	// 집과 나무는 연기 너머로도 윤곽이 보여야 한다. 땅보다 조금 밝은 하한을 둔다.
	const float brightness = std::max(kFogBrightness * 1.35f, TotalBrightness(worldX, worldY));
	return std::min(1.0f, brightness * 1.15f);
}

void Game::RecordExplored()
{
	// 지금 화면에서 밝혀진 칸을 기억한다. 사물이 드러나는 밝기와 같은 문턱을 써서
	// "무엇이 있는지 본 곳"만 지도에 남긴다.
	const int firstTileX = FloorToInt((m_CameraX - m_WindowWidth * 0.5f) / kTileSize);
	const int lastTileX  = FloorToInt((m_CameraX + m_WindowWidth * 0.5f) / kTileSize);
	const int firstTileY = FloorToInt((m_CameraY - m_WindowHeight * 0.5f) / kTileSize);
	const int lastTileY  = FloorToInt((m_CameraY + m_WindowHeight * 0.5f) / kTileSize);

	for (int tileY = firstTileY; tileY <= lastTileY; ++tileY)
	{
		for (int tileX = firstTileX; tileX <= lastTileX; ++tileX)
		{
			if (TotalBrightness(TileToWorld(tileX), TileToWorld(tileY)) >= kRevealAt)
			{
				m_Explored.Mark(tileX, tileY);
			}
		}
	}
}

bool Game::IsPlayerBlocked(float worldX, float worldY) const
{
	// 플레이어 사각형의 네 모서리를 검사한다.
	// 사각형끼리의 판정이라 렌더러가 그리는 것과 같은 수학이다.
	const float h = m_PlayerHalfSize;
	return IsBlockedAtWorld(worldX - h, worldY - h)
	    || IsBlockedAtWorld(worldX + h, worldY - h)
	    || IsBlockedAtWorld(worldX - h, worldY + h)
	    || IsBlockedAtWorld(worldX + h, worldY + h);
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

	const float step = Progression::MoveSpeed(m_Combat.level) * deltaSeconds;
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

const Regions::Region* Game::FindTalkableSleeper() const
{
	if (m_Companion.IsAwake())
	{
		return NULL;   // 한 번에 한 사람만 데리고 다닌다
	}

	for (const Regions::Region& region : m_NearbyRegions)
	{
		// 생기가 다 돌아와야 말을 알아듣는다.
		// 불을 들이대고 버티는 2초가 이 게임에서 사람을 되찾는 시간이다.
		if (region.woken || region.warmth < 1.0f)
		{
			continue;
		}

		const float sleeperX = TileToWorld(region.sleeperTileX);
		const float sleeperY = TileToWorld(region.sleeperTileY);
		if (Distance(m_PlayerX, m_PlayerY, sleeperX, sleeperY) <= kWakeReach)
		{
			return &region;
		}
	}

	return NULL;
}

bool Game::TryStartTalk()
{
	const Regions::Region* region = FindTalkableSleeper();
	if (region == NULL)
	{
		return false;
	}

	StartDialogue(*region);
	return true;
}

void Game::StartDialogue(const Regions::Region& region)
{
	const float sleeperX = TileToWorld(region.sleeperTileX);
	const float sleeperY = TileToWorld(region.sleeperTileY);
	const float towerX = TileToWorld(region.towerTileX);
	const float towerY = TileToWorld(region.towerTileY);

	const DialogueScript& script = WakeScript(static_cast<int>(region.personSeed % WakeScriptCount()));
	const std::string direction = DirectionName(towerX - sleeperX, towerY - sleeperY);

	m_Dialogue = DialogueState();
	m_Dialogue.active = true;
	m_Dialogue.regionX = region.regionX;
	m_Dialogue.regionY = region.regionY;

	for (int i = 0; i < script.lineCount; ++i)
	{
		DialoguePage page;
		page.fromPlayer = script.lines[i].fromPlayer;
		page.speaker = page.fromPlayer ? "나" : script.speaker;
		page.text = ReplaceAll(script.lines[i].text, "{종탑}", direction);
		m_Dialogue.pages.push_back(page);
	}

	// 말을 거는 동안에는 멈춰 서서 상대를 바라본다.
	m_PlayerMoving = false;
	m_FacingX = (sleeperX >= m_PlayerX) ? 1.0f : -1.0f;
	m_RingProgress = 0.0f;
}

void Game::UpdateDialogue(float deltaSeconds, const Input& input)
{
	const bool interact = input.Interact();
	const bool pressed = interact && !m_PrevInteract;
	m_PrevInteract = interact;
	m_PrevPlaceFire = input.PlaceFire();   // 대화 중에 누른 E가 끝난 뒤 화톳불로 새지 않게

	const DialoguePage& page = m_Dialogue.pages[m_Dialogue.page];
	const float total = static_cast<float>(CountCharacters(page.text));
	m_Dialogue.revealed = std::min(total, m_Dialogue.revealed + deltaSeconds * kCharsPerSecond);

	if (!pressed)
	{
		return;
	}

	// 아직 적히는 중이면 한 번에 끝까지 보여 준다. 다 보였으면 다음 페이지로.
	if (m_Dialogue.revealed < total)
	{
		m_Dialogue.revealed = total;
		return;
	}

	++m_Dialogue.page;
	m_Dialogue.revealed = 0.0f;

	if (m_Dialogue.page >= m_Dialogue.pages.size())
	{
		FinishDialogue();
	}
}

void Game::FinishDialogue()
{
	const Regions::Region region = m_Regions.At(m_Dialogue.regionX, m_Dialogue.regionY);
	m_Dialogue = DialogueState();

	// 대화가 끝나야 비로소 일어나 따라나선다.
	// 이제부터는 플레이어가 지나온 궤적을 따라 걷는다.
	m_Companion.Wake(TileToWorld(region.sleeperTileX), TileToWorld(region.sleeperTileY),
	                 region.regionX, region.regionY);
	m_Regions.MarkWoken(region.regionX, region.regionY);

	// 방금 일어난 사람이 같은 프레임에 굳은 형상으로도 그려지지 않도록 갱신한다.
	RefreshNearbyRegions();
}

void Game::UpdateRinging(float deltaSeconds, bool holding)
{
	// 큰 종은 혼자 울릴 수 없다.
	// 플레이어와 동행자가 함께 종탑에 붙어 있어야 게이지가 오른다.
	bool canRing = false;
	int ringingRegionX = 0;
	int ringingRegionY = 0;
	m_RingAvailable = false;
	m_WardenBlocking = false;

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
				// 종탑지기가 살아 있는 동안에는 밧줄에 손을 대지 못한다.
				if (!region.wardenDefeated)
				{
					m_WardenBlocking = true;
					m_RingTowerX = towerX;
					m_RingTowerY = towerY;
					break;
				}

				canRing = true;
				ringingRegionX = region.regionX;
				ringingRegionY = region.regionY;
				m_RingAvailable = true;
				m_RingTowerX = towerX;
				m_RingTowerY = towerY;
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

	if (interactPressed && TryStartTalk())
	{
		m_PrevInteract = interact;
		m_PrevPlaceFire = input.PlaceFire();
		return;   // 대화가 열린 프레임에는 종을 당기거나 화톳불을 놓지 않는다
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

void Game::BuildLights()
{
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
}

void Game::Update(float deltaSeconds, const Input& input)
{
	m_Time += deltaSeconds;
	m_MapAge += deltaSeconds;

	// 대화 중에는 세계가 멈춘다. 기름도 줄지 않고, 발자국도 식지 않고,
	// 굳은 사람의 생기도 빠지지 않는다. 불꽃과 까마귀는 계속 움직인다.
	if (m_Dialogue.active)
	{
		UpdateDialogue(deltaSeconds, input);
		BuildLights();
		return;
	}

	if (!m_PendingRespawn)
	{
		MovePlayer(deltaSeconds, input);
	}
	else
	{
		m_PlayerMoving = false;
	}
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

	BuildLights();
	RecordExplored();

	// 여기서부터 밝기 질의는 이번 프레임의 등불을 반영한다.
	// 굳은 사람과 상호작용 판정이 한 프레임 늦지 않도록 조명을 먼저 세운다.
	UpdateSleepers(deltaSeconds);
	HandleInteraction(deltaSeconds, input);
	UpdateCombat(deltaSeconds, input);

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
	const bool talking = m_Dialogue.active
	                  && m_Dialogue.regionX == region.regionX && m_Dialogue.regionY == region.regionY;
	look.posture = talking ? 1.0f : warmth * 0.85f;
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
	DrawCombatHud(renderer);
	DrawFloatingTexts(renderer);

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

	if (m_ShowMinimap)
	{
		DrawMinimap(renderer);
	}

	if (m_Dialogue.active)
	{
		DrawDialogue(renderer);
	}
	else
	{
		DrawPrompts(renderer);
	}
}

void Game::RebuildMinimap(Renderer* renderer)
{
	if (m_MapImage == 0)
	{
		m_MapImage = renderer->CreateImage(kMapTiles, kMapTiles);
		m_MapPixels.assign(static_cast<size_t>(kMapTiles) * kMapTiles * 4, 0);
	}

	// 플레이어를 가운데 두고 사방 48칸을 그린다.
	m_MapOriginX = FloorToInt(m_PlayerX / kTileSize) - kMapTiles / 2;
	m_MapOriginY = FloorToInt(m_PlayerY / kTileSize) - kMapTiles / 2;

	for (int row = 0; row < kMapTiles; ++row)
	{
		for (int column = 0; column < kMapTiles; ++column)
		{
			const int tileX = m_MapOriginX + column;
			const int tileY = m_MapOriginY + row;
			unsigned char* pixel = &m_MapPixels[(static_cast<size_t>(row) * kMapTiles + column) * 4];

			// 밝혀 본 적 없는 곳은 지도에도 없다.
			if (!m_Explored.IsSeen(tileX, tileY))
			{
				pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
				continue;
			}

			const unsigned char tile = m_World.TileAt(tileX, tileY);
			const float* color = kMapGrass;
			if (tile == World::kPath)       { color = kMapPath; }
			else if (tile == World::kPlaza) { color = kMapPlaza; }
			else if (tile == World::kHouse) { color = kMapHouse; }
			else if (tile == World::kTree)  { color = kMapTree; }

			const float jitter = static_cast<float>((tileX * 31 + tileY * 17) & 3) * 0.012f;
			pixel[0] = ToByte(color[0] + jitter);
			pixel[1] = ToByte(color[1] + jitter);
			pixel[2] = ToByte(color[2] + jitter);
			pixel[3] = 235;
		}
	}

	renderer->UpdateImage(m_MapImage, kMapTiles, kMapTiles, m_MapPixels.data());
	m_MapVersion = m_Explored.Version();
	m_MapAge = 0.0f;
	m_MapReady = true;
}

void Game::DrawMinimap(Renderer* renderer)
{
	const int playerTileX = FloorToInt(m_PlayerX / kTileSize);
	const int playerTileY = FloorToInt(m_PlayerY / kTileSize);
	const int drift = std::max(std::abs(playerTileX - (m_MapOriginX + kMapTiles / 2)),
	                           std::abs(playerTileY - (m_MapOriginY + kMapTiles / 2)));

	// 멀리 걸어가 가장자리가 비기 전에, 또는 새로 밝힌 곳이 생겼을 때 다시 그린다.
	// 새로 밝히는 동안 매 프레임 다시 그리지 않도록 최소 간격을 둔다.
	if (!m_MapReady || drift > 12 || (m_Explored.Version() != m_MapVersion && m_MapAge >= 0.15f))
	{
		RebuildMinimap(renderer);
	}

	const float half = kMapViewSize * 0.5f;
	const float centerX = m_WindowWidth * 0.5f - 18.0f - half;
	const float centerY = m_WindowHeight * 0.5f - 18.0f - half;
	const float scale = kMapPixelsPerTile / kTileSize;   // 월드 픽셀 → 지도 픽셀

	auto mapX = [&](float worldX) { return centerX + (worldX - m_PlayerX) * scale; };
	auto mapY = [&](float worldY) { return centerY - (worldY - m_PlayerY) * scale; };

	// ── 틀 ──
	const Color rim = { kBronze[0], kBronze[1], kBronze[2], 0.45f };
	const Color backing = { 0.015f, 0.017f, 0.025f, 0.86f };
	renderer->DrawRoundRect(centerX, centerY, kMapViewSize + 8.0f, kMapViewSize + 8.0f, 8.0f, rim);
	renderer->DrawRoundRect(centerX, centerY, kMapViewSize + 4.0f, kMapViewSize + 4.0f, 6.0f, backing);

	renderer->SetClip(centerX - half, centerY - half, kMapViewSize, kMapViewSize);

	// ── 밝혀 본 땅 ── 플레이어가 움직이면 이미지가 부드럽게 흘러간다
	const float imageSize = kMapTiles * kMapPixelsPerTile;
	renderer->DrawImage(m_MapImage,
	                    mapX(static_cast<float>(m_MapOriginX * kTileSize)),
	                    mapY(static_cast<float>(m_MapOriginY * kTileSize)),
	                    imageSize, imageSize);

	// ── 표식 ── 밝혀 본 곳에 있던 것만 남긴다
	const int reach = static_cast<int>(half / kMapPixelsPerTile) + 2;
	const int tileMinX = playerTileX - reach;
	const int tileMaxX = playerTileX + reach;
	const int tileMinY = playerTileY - reach;
	const int tileMaxY = playerTileY + reach;

	m_MapProps.clear();
	m_World.CollectProps(tileMinX, tileMinY, tileMaxX, tileMaxY, m_MapProps);
	for (const World::Prop& prop : m_MapProps)
	{
		if (!m_Explored.IsSeen(prop.tileX, prop.tileY))
		{
			continue;
		}

		const int marker = (prop.kind == World::kPropSupply) ? kMarkSupply
		                 : (prop.kind == World::kPropBarrel) ? kMarkBarrel
		                 : kMarkRubble;
		DrawMapMarker(renderer, marker, mapX(TileToWorld(prop.tileX)), mapY(TileToWorld(prop.tileY)), 0.0f);
	}

	m_MapRegions.clear();
	m_Regions.CollectOverlapping(tileMinX, tileMinY, tileMaxX, tileMaxY, m_MapRegions);
	for (const Regions::Region& region : m_MapRegions)
	{
		// 굳은 사람은 본 적이 있을 때만
		if (!region.woken && m_Explored.IsSeen(region.sleeperTileX, region.sleeperTileY))
		{
			DrawMapMarker(renderer, kMarkSleeper,
			              mapX(TileToWorld(region.sleeperTileX)), mapY(TileToWorld(region.sleeperTileY)),
			              region.warmth);
		}

		// 종탑은 본 적이 있거나, 그 마을 사람에게서 위치를 들었으면 표시한다.
		if (region.rung || region.woken || m_Explored.IsSeen(region.towerTileX, region.towerTileY))
		{
			DrawMapMarker(renderer, region.rung ? kMarkTowerRung : kMarkTowerSilent,
			              mapX(TileToWorld(region.towerTileX)), mapY(TileToWorld(region.towerTileY)), 0.0f);
		}
	}

	for (const StaticLight& fire : m_Fires)
	{
		DrawMapMarker(renderer, kMarkFire, mapX(fire.x), mapY(fire.y), 0.0f);
	}

	if (m_Companion.IsAwake())
	{
		DrawMapMarker(renderer, kMarkCompanion, mapX(m_Companion.X()), mapY(m_Companion.Y()), 0.0f);
	}

	// 적. 종탑지기는 따로 크게 표시한다.
	for (const Enemy& enemy : m_Enemies)
	{
		if (enemy.dying > 0.0f)
		{
			continue;
		}
		DrawMapMarker(renderer, enemy.kind == EnemyKind::Warden ? kMarkWarden : kMarkEnemy,
		              mapX(enemy.x), mapY(enemy.y), 0.0f);
	}

	// 나 — 바라보는 쪽을 가리키는 화살표
	const Color outline = { 0.01f, 0.01f, 0.02f, 0.95f };
	const Color me = { 0.97f, 0.92f, 0.80f, 1.0f };
	const float pointing = -1.5708f * m_FacingX;
	renderer->DrawTriangle(centerX, centerY, 12.0f, 12.0f, outline, pointing);
	renderer->DrawTriangle(centerX, centerY, 8.0f, 8.5f, me, pointing);

	renderer->ClearClip();

	// ── 범례 ──
	const float legendTop = centerY - half - 20.0f;
	const float rowHeight = 17.0f;
	const Color legendBacking = { 0.015f, 0.017f, 0.025f, 0.72f };
	const Color legendText = { 0.78f, 0.76f, 0.71f, 1.0f };
	renderer->DrawRoundRect(centerX, legendTop - rowHeight * 2.0f, kMapViewSize + 8.0f, rowHeight * 5.0f + 10.0f,
	                        6.0f, legendBacking);

	const int markers[10] = { kMarkTowerSilent, kMarkTowerRung, kMarkSleeper, kMarkCompanion,
	                          kMarkFire, kMarkSupply, kMarkBarrel, kMarkRubble,
	                          kMarkWarden, kMarkEnemy };
	const char* const labels[10] = { "종탑", "울린 종탑", "굳은 사람", "동행자",
	                                 "화톳불", "보급품", "나무통", "잔해",
	                                 "종탑지기", "적" };

	for (int i = 0; i < 10; ++i)
	{
		const float itemX = centerX - half + 10.0f + (i % 2) * half;
		const float itemY = legendTop - (i / 2) * rowHeight;
		DrawMapMarker(renderer, markers[i], itemX, itemY, 0.0f);
		renderer->DrawLabel(labels[i], 12.0f, itemX + 11.0f, itemY + 8.0f, legendText);
	}
}

void Game::DrawPrompt(Renderer* renderer, const std::string& text, float centerX, float bottomY)
{
	// 머리 위에 뜨는 짧은 안내. 바탕을 깔아 밝은 곳에서도 읽히게 한다.
	const float size = 15.0f;
	const float width = renderer->MeasureLabel(text, size);
	if (width <= 0.0f)
	{
		return;
	}

	const float bob = std::sin(m_Time * 3.0f) * 1.0f;
	const float centerY = bottomY + 13.0f + bob;
	const Color backing = { 0.02f, 0.022f, 0.03f, 0.78f };
	const Color edge = { kFlame[0], kFlame[1], kFlame[2], 0.35f };

	renderer->DrawRoundRect(centerX, centerY, width + 20.0f, 27.0f, 13.5f, edge);
	renderer->DrawRoundRect(centerX, centerY, width + 18.0f, 25.0f, 12.5f, backing);
	renderer->DrawLabel(text, size, centerX - width * 0.5f, centerY + 11.0f,
	                    Tint(kFlameCore, 0.95f));
}

void Game::DrawPrompts(Renderer* renderer)
{
	// 말을 걸 수 있는 사람
	if (const Regions::Region* region = FindTalkableSleeper())
	{
		DrawPrompt(renderer, "Space  말 걸기",
		           ToRenderX(TileToWorld(region->sleeperTileX)),
		           ToRenderY(TileToWorld(region->sleeperTileY)) + 50.0f);
	}

	// 함께 종을 당길 수 있는 종탑
	if (m_RingAvailable)
	{
		DrawPrompt(renderer, m_RingProgress > 0.0f ? "놓지 마세요" : "Space 누르고 있기  종 울리기",
		           ToRenderX(m_RingTowerX), ToRenderY(m_RingTowerY) + 108.0f);
	}

	if (m_WardenBlocking)
	{
		DrawPrompt(renderer, "종탑지기를 먼저 쓰러뜨려야 한다",
		           ToRenderX(m_RingTowerX), ToRenderY(m_RingTowerY) + 108.0f);
	}

	// 다시 굳어버린 동행자
	if (m_Companion.IsFrozenAgain())
	{
		DrawPrompt(renderer, "불을 비춰 다시 깨우기",
		           ToRenderX(m_Companion.X()), ToRenderY(m_Companion.Y()) + 40.0f);
	}
}

void Game::DrawDialogue(Renderer* renderer)
{
	const DialoguePage& page = m_Dialogue.pages[m_Dialogue.page];

	// 화면 아래, HUD 줄 바로 위에 대화창을 둔다.
	const float panelWidth = 860.0f;
	const float panelHeight = 128.0f;
	const float centerY = -m_WindowHeight * 0.5f + 44.0f + panelHeight * 0.5f;
	const float left = -panelWidth * 0.5f + 30.0f;
	const float top = centerY + panelHeight * 0.5f;

	const Color rim = { kBronze[0], kBronze[1], kBronze[2], 0.38f };
	const Color panel = { 0.022f, 0.025f, 0.036f, 0.93f };
	renderer->DrawRoundRect(0.0f, centerY, panelWidth + 4.0f, panelHeight + 4.0f, 11.0f, rim);
	renderer->DrawRoundRect(0.0f, centerY, panelWidth, panelHeight, 9.0f, panel);

	// 말하는 사람. 깨어난 사람의 이름은 불빛 색, 내 대사는 가라앉은 색.
	const Color nameColor = page.fromPlayer ? Tint(kStone, 1.9f) : Tint(kFlame);
	renderer->DrawRoundRect(left + 3.0f, top - 24.0f, 6.0f, 6.0f, 1.0f, nameColor, 0.7854f);
	renderer->DrawLabel(page.speaker, 17.0f, left + 16.0f, top - 13.0f, nameColor);

	// 본문. 줄마다 앞 줄이 다 적힌 뒤에 적히기 시작한다.
	const Color textColor = page.fromPlayer
		? Color{ 0.72f, 0.74f, 0.78f, 1.0f }
		: Color{ 0.90f, 0.87f, 0.80f, 1.0f };

	const std::vector<std::string> lines = SplitLines(page.text);
	float before = 0.0f;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		const float count = static_cast<float>(CountCharacters(lines[i]));
		const float reveal = (count > 0.0f)
			? std::max(0.0f, std::min(1.0f, (m_Dialogue.revealed - before) / count))
			: 1.0f;
		renderer->DrawLabel(lines[i], 21.0f, left, top - 44.0f - i * 32.0f, textColor, reveal);
		before += count;
	}

	// 다 적혔으면 다음으로 넘기라는 표시가 깜빡인다. 마지막 페이지는 따라나선다는 뜻이다.
	if (m_Dialogue.revealed >= static_cast<float>(CountCharacters(page.text)))
	{
		const bool last = m_Dialogue.page + 1 >= m_Dialogue.pages.size();
		const float blink = 0.55f + 0.45f * std::sin(m_Time * 5.0f);
		const float right = panelWidth * 0.5f - 30.0f;
		const float bottom = centerY - panelHeight * 0.5f + 20.0f;
		const std::string hint = last ? "Space  함께 가기" : "Space";
		const float hintWidth = renderer->MeasureLabel(hint, 14.0f);

		renderer->DrawLabel(hint, 14.0f, right - hintWidth - 16.0f, bottom + 9.0f,
		                    Tint(kStone, 1.6f, blink));
		renderer->DrawTriangle(right - 4.0f, bottom + 1.0f + blink * 2.0f, 10.0f, 8.0f,
		                       Tint(kFlame, 1.0f, blink), 3.14159265f);
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
	// 배경색 자체가 연기다. 땅을 아주 어둡게 깔아 길과 광장의 윤곽만 남긴다.
	// 되찾은 구역 안에서는 이 하한이 종탑의 밝기까지 올라간다.
	for (int tileY = firstTileY; tileY <= lastTileY; ++tileY)
	{
		for (int tileX = firstTileX; tileX <= lastTileX; ++tileX)
		{
			const float worldX = TileToWorld(tileX);
			const float worldY = TileToWorld(tileY);
			const float brightness = std::max(kFogBrightness, BeaconBrightness(worldX, worldY));

			float ground[3];
			GroundColor(m_World.TileAt(tileX, tileY), tileX, tileY, ground);

			renderer->DrawSolidRect(ToRenderX(worldX), ToRenderY(worldY),
			                        0.0f, static_cast<float>(kTileSize),
			                        ground[0] * brightness,
			                        ground[1] * brightness,
			                        ground[2] * brightness,
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

			const int tileX = FloorToInt(worldX / kTileSize);
			const int tileY = FloorToInt(worldY / kTileSize);
			float ground[3];
			GroundColor(m_World.TileAt(tileX, tileY), tileX, tileY, ground);

			// 불이 직접 비추는 곳은 따뜻하게, 남은 발자국은 차갑게 식어간다.
			const float warmth = m_Light.Glow(cellX, cellY) * 0.35f;

			renderer->DrawSolidRect(
				ToRenderX(worldX), ToRenderY(worldY), 0.0f, kLightCell,
				Mix(ground[0], kFlame[0], warmth) * dynamic,
				Mix(ground[1], kFlame[1], warmth) * dynamic,
				Mix(ground[2], kFlame[2], warmth) * dynamic,
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

	// 집과 나무. 연기 너머로 윤곽이 보이는 구조물이다. 사람과 같이 발밑으로 정렬해,
	// 집 앞을 지나면 집을 가리고 집 뒤로 돌아가면 지붕에 가려진다.
	m_VisibleHouses.clear();
	m_VisibleTrees.clear();
	m_World.CollectStructures(firstTileX - 5, firstTileY - 1, lastTileX + 5, lastTileY + 5,
	                          m_VisibleHouses, m_VisibleTrees);

	for (size_t i = 0; i < m_VisibleHouses.size(); ++i)
	{
		const World::House& house = m_VisibleHouses[i];
		const float leftX = static_cast<float>(house.tileX * kTileSize);
		const float rightX = leftX + house.width * kTileSize;
		const float bottomY = static_cast<float>((house.tileY + house.depth) * kTileSize);
		const float height = house.depth * kTileSize + 22.0f;   // 지붕 끝까지

		if (rightX > worldLeft - 16.0f && leftX < worldRight + 16.0f
		 && bottomY > worldTop - 8.0f && bottomY < worldBottom + height)
		{
			m_Drawables.push_back({ bottomY, kDrawHouse, static_cast<int>(i) });
		}
	}

	for (size_t i = 0; i < m_VisibleTrees.size(); ++i)
	{
		const float worldX = TileToWorld(m_VisibleTrees[i].tileX);
		const float worldY = TileToWorld(m_VisibleTrees[i].tileY) + 6.0f;
		if (inView(worldX, worldY))
		{
			m_Drawables.push_back({ worldY, kDrawTree, static_cast<int>(i) });
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

	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		if (inView(m_Enemies[i].x, m_Enemies[i].y))
		{
			m_Drawables.push_back({ m_Enemies[i].y, kDrawEnemy, static_cast<int>(i) });
		}
	}

	m_Drawables.push_back({ m_PlayerY, kDrawPlayer, 0 });

	// 떨어진 물건은 땅 위에 놓여 있으므로 서 있는 것들보다 먼저 그린다.
	DrawPickups(renderer);

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

		case kDrawHouse:
		{
			const World::House& house = m_VisibleHouses[item.index];
			const float left = static_cast<float>(house.tileX * kTileSize);
			const float bottom = static_cast<float>((house.tileY + house.depth) * kTileSize);
			const float frontX = left + house.width * kTileSize * 0.5f;
			const float frontY = bottom - kTileSize * 0.5f;

			const float warmth = m_Light.Glow(m_Light.CellOf(frontX), m_Light.CellOf(frontY)) * 0.3f;
			const bool lived = BeaconBrightness(frontX, frontY) > 0.0f;
			DrawHouse(renderer, ToRenderX(left), ToRenderY(bottom), house,
			          StructureShade(frontX, frontY), warmth, lived, m_Time);
			break;
		}

		case kDrawTree:
		{
			const World::Tree& tree = m_VisibleTrees[item.index];
			const float worldX = TileToWorld(tree.tileX);
			const float worldY = TileToWorld(tree.tileY) + 6.0f;
			const float warmth = m_Light.Glow(m_Light.CellOf(worldX), m_Light.CellOf(worldY)) * 0.3f;
			DrawTree(renderer, ToRenderX(worldX), ToRenderY(worldY), tree,
			         StructureShade(worldX, worldY), warmth, m_Time);
			break;
		}

		case kDrawEnemy:
			DrawEnemy(renderer, m_Enemies[item.index]);
			break;
		}
	}

	// 날아가는 것들은 모두의 머리 위로 그린다.
	DrawProjectiles(renderer);

	// 레벨이 오른 순간, 발밑에서 영혼의 빛이 번져 나간다.
	if (m_Combat.levelUpGlow > 0.0f)
	{
		const float glow = m_Combat.levelUpGlow;
		const Color ring = { kSoulColor[0], kSoulColor[1], kSoulColor[2], glow * 0.35f };
		renderer->DrawCircle(ToRenderX(m_PlayerX), ToRenderY(m_PlayerY) + 14.0f,
		                     22.0f + (1.0f - glow) * 46.0f, ring, glow * 2.0f, 8.0f);
	}
}

void Game::RenderHud(Renderer* renderer)
{
	DrawHud(renderer);
}

// ═══════════════════════════════════════════════════════════════
//  전투 그리기
// ═══════════════════════════════════════════════════════════════

void Game::DrawEnemy(Renderer* renderer, const Enemy& enemy)
{
	const float x = ToRenderX(enemy.x);
	const float y = ToRenderY(enemy.y);

	// 쓰러지면 연기처럼 흩어진다
	const float fade = (enemy.dying > 0.0f) ? std::max(0.0f, 1.0f - enemy.dying / 0.45f) : 1.0f;
	if (fade <= 0.0f)
	{
		return;
	}

	const float flash = (enemy.hitFlash > 0.0f) ? 0.85f : 0.0f;
	const float bob = std::sin(m_Time * 3.0f + enemy.phase) * 1.5f;
	const float facing = (m_PlayerX >= enemy.x) ? 1.0f : -1.0f;

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
		const float sway = std::sin(m_Time * 4.0f + enemy.phase) * 2.0f;
		renderer->DrawEllipse(x, y, 10.0f, 3.5f, shadow, 0.0f, 0.0f, 2.5f);
		renderer->DrawTriangle(x + sway, y + 10.0f + bob, 20.0f, 18.0f, body(kShadeBody, 1.0f), 3.14159f);
		renderer->DrawEllipse(x, y + 21.0f + bob, 11.0f, 12.0f, body(kShadeBody, 1.0f));
		renderer->DrawEllipse(x - 9.0f, y + 15.0f + bob * 1.3f, 3.5f, 6.5f, body(kShadeBody, 1.2f), 0.5f + sway * 0.05f);
		renderer->DrawEllipse(x + 9.0f, y + 15.0f - bob * 1.3f, 3.5f, 6.5f, body(kShadeBody, 1.2f), -0.5f + sway * 0.05f);

		const Color eye = { kEyePale[0], kEyePale[1], kEyePale[2], fade };
		renderer->DrawCircle(x - 3.5f + facing * 1.5f, y + 23.0f + bob, 1.8f, eye, kGlowEye * fade);
		renderer->DrawCircle(x + 3.5f + facing * 1.5f, y + 23.0f + bob, 1.8f, eye, kGlowEye * fade);
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
		look.posture = 0.3f + 0.08f * std::sin(m_Time * 2.0f + enemy.phase);
		look.facing = facing;
		look.bob = std::fabs(bob);
		DrawFigure(renderer, x, y, look);

		// DrawFigure의 머리 위치와 같은 계산
		const float height = Mix(0.70f, 1.0f, look.posture);
		const float hunch = 1.0f - look.posture;
		const float headX = x + facing * hunch * 5.5f;
		const float headY = y + (26.0f - hunch * 3.0f) * height + look.bob;
		const Color eye = { kEyeRed[0], kEyeRed[1], kEyeRed[2], fade };
		renderer->DrawCircle(headX + facing * 0.4f, headY + 0.5f, 1.2f, eye, kGlowEye * fade);
		renderer->DrawCircle(headX + facing * 3.4f, headY + 0.5f, 1.2f, eye, kGlowEye * fade);
		break;
	}

	case EnemyKind::Warden:
	{
		// 종탑지기 — 녹슨 종을 가슴에 매단 거대한 두건의 그림자. 까마귀가 곁을 맴돈다
		const float engaged = enemy.engaged ? 1.0f : 0.0f;
		const Color aura = { 0.06f, 0.02f, 0.09f, (0.28f + 0.18f * engaged) * fade };

		renderer->DrawEllipse(x, y, 30.0f, 9.0f, shadow, 0.0f, 0.0f, 4.0f);
		renderer->DrawCircle(x, y + 40.0f, 46.0f, aura, 0.0f, 16.0f);

		renderer->DrawTriangle(x, y + 30.0f + bob * 0.5f, 60.0f, 60.0f, body(kWardenRobe, 1.0f));
		renderer->DrawRoundRect(x, y + 51.0f + bob, 48.0f, 17.0f, 8.0f, body(kWardenRobe, 1.2f));

		// 몸을 감은 쇠사슬
		const Color chain = body(kRust, 0.8f);
		renderer->DrawSegment(x - 21.0f, y + 49.0f + bob, x + 15.0f, y + 22.0f + bob, 2.2f, chain);
		renderer->DrawSegment(x + 21.0f, y + 49.0f + bob, x - 15.0f, y + 20.0f + bob, 2.2f, chain);

		// 가슴의 녹슨 종
		renderer->DrawEllipse(x, y + 35.0f + bob, 8.0f, 9.0f, body(kRust, 1.0f));
		renderer->DrawRoundRect(x, y + 27.5f + bob, 19.0f, 3.0f, 1.5f, body(kRust, 0.8f));

		// 두건과 텅 빈 얼굴, 그 안의 붉은 눈
		renderer->DrawTriangle(x, y + 82.0f + bob, 18.0f, 16.0f, body(kWardenRobe, 1.1f));
		renderer->DrawEllipse(x, y + 67.0f + bob, 15.5f, 15.0f, body(kWardenRobe, 1.1f));
		const Color hollow = { 0.0f, 0.0f, 0.0f, fade };
		renderer->DrawEllipse(x + facing * 1.5f, y + 65.0f + bob, 9.0f, 8.0f, hollow);

		const Color eye = { kEyeRed[0], kEyeRed[1], kEyeRed[2], fade };
		const float eyeGlow = kGlowEye * (1.0f + engaged) * fade;
		renderer->DrawCircle(x + facing * 1.5f - 3.8f, y + 66.0f + bob, 2.2f, eye, eyeGlow);
		renderer->DrawCircle(x + facing * 1.5f + 3.8f, y + 66.0f + bob, 2.2f, eye, eyeGlow);

		for (int k = 0; k < 2; ++k)
		{
			const float angle = m_Time * 1.4f + enemy.phase + k * 3.14159f;
			DrawCrow(renderer, x + std::cos(angle) * 40.0f, y + 74.0f + std::sin(angle) * 10.0f,
			         std::sin(angle) > 0.0f ? -1.0f : 1.0f, m_Time, enemy.phase + k);
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
		renderer->DrawRoundRect(x, barY, 26.0f, 4.5f, 2.0f, back);
		if (ratio > 0.0f)
		{
			renderer->DrawRoundRect(x - 12.0f * (1.0f - ratio), barY, 24.0f * ratio, 2.5f, 1.2f, Tint(kEyeRed, 0.9f));
		}
	}
}

void Game::DrawPickups(Renderer* renderer)
{
	for (const Pickup& pickup : m_Pickups)
	{
		const float x = ToRenderX(pickup.x);
		const float y = ToRenderY(pickup.y);
		if (x < -m_WindowWidth * 0.5f - 20.0f || x > m_WindowWidth * 0.5f + 20.0f
		 || y < -m_WindowHeight * 0.5f - 20.0f || y > m_WindowHeight * 0.5f + 20.0f)
		{
			continue;
		}

		// 오래 놓여 있던 것은 사라지기 전에 깜빡인다
		const float remaining = 90.0f - pickup.age;
		const float blink = (remaining < 5.0f && std::fmod(pickup.age * 6.0f, 1.0f) < 0.5f) ? 0.35f : 1.0f;
		const float bob = std::sin(m_Time * 3.0f + pickup.x * 0.05f) * 1.5f;
		const float cy = y + 7.0f + bob;

		const Color shadow = { 0.0f, 0.0f, 0.0f, 0.3f * blink };
		renderer->DrawEllipse(x, y, 5.5f, 2.0f, shadow, 0.0f, 0.0f, 1.5f);

		switch (pickup.kind)
		{
		case PickupKind::Soul:
		{
			// 영혼석 — 푸르게 빛나는 마름모
			const float size = 6.0f + std::min(4.0f, pickup.value * 0.5f);
			renderer->DrawRoundRect(x, cy, size, size, 1.0f, Tint(kSoulColor, 1.0f, blink), 0.7854f, kGlowSoul * blink);
			renderer->DrawRoundRect(x, cy, size * 0.4f, size * 0.4f, 0.5f, Tint(kFlameCore, 1.0f, blink), 0.7854f, kGlowSoul * blink);
			break;
		}

		case PickupKind::Heal:
		{
			// 회복약 — 붉은 병 위의 흰 십자
			const Color white = { 0.95f, 0.93f, 0.90f, blink };
			renderer->DrawCircle(x, cy, 6.5f, Tint(kHealRed, 1.0f, blink), 1.0f * blink);
			renderer->DrawRoundRect(x, cy, 7.0f, 2.4f, 1.0f, white);
			renderer->DrawRoundRect(x, cy, 2.4f, 7.0f, 1.0f, white);
			break;
		}

		case PickupKind::Weapon:
		{
			// 강화석 — 금빛 무리 속의 짧은 칼
			const Color halo = { kGold[0], kGold[1], kGold[2], 0.25f * blink };
			renderer->DrawCircle(x, cy, 10.0f, halo, 0.0f, 3.0f);
			renderer->DrawSegment(x - 3.5f, cy - 4.5f, x + 5.0f, cy + 5.0f, 2.4f, Tint(kSteel, 1.0f, blink), 1.5f * blink);
			renderer->DrawSegment(x - 5.5f, cy - 0.5f, x - 0.5f, cy - 6.0f, 2.0f, Tint(kGold, 1.0f, blink), 2.0f * blink);
			renderer->DrawSegment(x - 3.5f, cy - 4.5f, x - 6.0f, cy - 7.0f, 2.2f, Tint(kWood, 1.0f, blink));
			break;
		}

		case PickupKind::Magnet:
		{
			// 자석 — 붉은 말굽 모양
			const Color red = Tint(kMagnetRed, 1.0f, blink);
			const Color tip = Tint(kSteel, 1.0f, blink);
			renderer->DrawRoundRect(x, cy - 4.0f, 11.0f, 4.0f, 2.0f, red, 0.0f, 1.2f * blink);
			renderer->DrawSegment(x - 4.0f, cy - 4.0f, x - 4.0f, cy + 4.0f, 3.2f, red, 1.2f * blink);
			renderer->DrawSegment(x + 4.0f, cy - 4.0f, x + 4.0f, cy + 4.0f, 3.2f, red, 1.2f * blink);
			renderer->DrawRoundRect(x - 4.0f, cy + 5.2f, 3.6f, 2.4f, 0.6f, tip);
			renderer->DrawRoundRect(x + 4.0f, cy + 5.2f, 3.6f, 2.4f, 0.6f, tip);
			break;
		}
		}
	}
}

void Game::DrawProjectiles(Renderer* renderer)
{
	// 등불의 불씨 — 날아온 길에 꼬리가 남는다
	for (const Projectile& projectile : m_Projectiles)
	{
		const float x = ToRenderX(projectile.x);
		const float y = ToRenderY(projectile.y);
		const float speed = std::sqrt(projectile.vx * projectile.vx + projectile.vy * projectile.vy);
		const float backX = (speed > 0.0f) ? -projectile.vx / speed : 0.0f;
		const float backY = (speed > 0.0f) ? projectile.vy / speed : 0.0f;   // 렌더러는 y가 위쪽

		for (int k = 3; k >= 1; --k)
		{
			renderer->DrawCircle(x + backX * k * 5.0f, y + backY * k * 5.0f, 3.2f - k * 0.6f,
			                     Tint(kEmber, 1.0f, 0.55f - k * 0.12f), kGlowEmber * 0.35f);
		}
		renderer->DrawCircle(x, y, 3.6f, Tint(kFlame), kGlowEmber);
		renderer->DrawCircle(x, y, 1.6f, Tint(kFlameCore), kGlowEmber * 1.4f);
	}

	// 종탑지기의 어둠 구슬 — 검은 속을 보랏빛 테가 둘러, 어둠 속에서도 피할 수 있게 보인다
	for (const Orb& orb : m_Orbs)
	{
		const float x = ToRenderX(orb.x);
		const float y = ToRenderY(orb.y);
		const float pulse = 0.8f + 0.2f * std::sin(m_Time * 12.0f + orb.x * 0.1f);
		const Color rim = { kOrbRim[0], kOrbRim[1], kOrbRim[2], 0.4f };
		renderer->DrawCircle(x, y, 8.0f * pulse, rim, 1.4f, 3.0f);
		renderer->DrawCircle(x, y, 5.0f, Tint(kOrbCore));
		renderer->DrawCircle(x, y, 1.8f, Tint(kOrbRim), 2.5f);
	}
}

float Game::MeasureDigits(Renderer* renderer, const std::string& text, float size)
{
	float width = 0.0f;
	for (char c : text)
	{
		width += std::max(0.0f, renderer->MeasureLabel(std::string(1, c), size) - 4.0f);
	}
	return width;
}

float Game::DrawDigits(Renderer* renderer, const std::string& text, float size,
                       float left, float top, const Color& color)
{
	// 숫자는 글자 하나씩 그린다. "85/130"처럼 계속 바뀌는 문자열을 통째로 그리면
	// 값마다 텍스처가 새로 만들어져 끝없이 쌓이지만, 글자 단위면 열몇 장으로 끝난다.
	float x = left;
	for (char c : text)
	{
		const std::string glyph(1, c);
		const float width = renderer->MeasureLabel(glyph, size);
		renderer->DrawLabel(glyph, size, x, top, color);
		x += std::max(0.0f, width - 4.0f);   // 글자마다 붙는 여백(양쪽 2px)을 뺀다
	}
	return x - left;
}

void Game::DrawFloatingTexts(Renderer* renderer)
{
	for (const FloatingText& text : m_FloatingTexts)
	{
		const float t = text.age / text.life;
		const float alpha = std::max(0.0f, 1.0f - t * t);
		const float x = ToRenderX(text.x);
		const float y = ToRenderY(text.y);
		const float top = y + text.size * 0.6f;

		const Color color = { text.r, text.g, text.b, alpha };
		const Color shadow = { 0.0f, 0.0f, 0.0f, alpha * 0.75f };

		if (text.number >= 0)
		{
			const std::string digits = std::to_string(text.number);
			const float width = MeasureDigits(renderer, digits, text.size);
			DrawDigits(renderer, digits, text.size, x - width * 0.5f + 1.0f, top - 1.0f, shadow);
			DrawDigits(renderer, digits, text.size, x - width * 0.5f, top, color);
		}
		else
		{
			const float width = renderer->MeasureLabel(text.label, text.size);
			renderer->DrawLabel(text.label, text.size, x - width * 0.5f + 1.0f, top - 1.0f, shadow);
			renderer->DrawLabel(text.label, text.size, x - width * 0.5f, top, color);
		}
	}
}

void Game::DrawCombatHud(Renderer* renderer)
{
	const float halfWidth = m_WindowWidth * 0.5f;
	const float halfHeight = m_WindowHeight * 0.5f;

	// ── 맞았을 때 화면이 붉게 번쩍인다 ──
	const float maxHealth = Progression::MaxHealth(m_Combat.level);
	const float ratio = std::max(0.0f, m_Combat.health / maxHealth);
	float redness = m_Combat.hurtFlash * 0.22f;
	if (ratio < 0.3f && !m_PendingRespawn)
	{
		redness += (0.3f - ratio) * 0.25f * (0.6f + 0.4f * std::sin(m_Time * 5.0f));   // 목숨이 위태로우면 맥박처럼
	}
	if (redness > 0.0f)
	{
		const Color red = { 0.55f, 0.04f, 0.04f, std::min(0.4f, redness) };
		renderer->DrawRoundRect(0.0f, 0.0f, static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight), 0.0f, red);
	}

	const Color backing = { 0.015f, 0.017f, 0.025f, 0.86f };
	const Color rim = { kBronze[0], kBronze[1], kBronze[2], 0.55f };
	const Color text = { 0.92f, 0.90f, 0.84f, 1.0f };
	const Color dim = { 0.62f, 0.60f, 0.56f, 1.0f };

	const float left = -halfWidth + 20.0f;
	const float top = halfHeight - 18.0f;

	// ── 레벨 ──
	const float badgeX = left + 20.0f;
	const float badgeY = top - 20.0f;
	if (m_Combat.levelUpGlow > 0.0f)
	{
		const Color glow = { kSoulColor[0], kSoulColor[1], kSoulColor[2], m_Combat.levelUpGlow * 0.5f };
		renderer->DrawCircle(badgeX, badgeY, 21.0f + m_Combat.levelUpGlow * 8.0f, glow, 0.0f, 4.0f);
	}
	renderer->DrawCircle(badgeX, badgeY, 21.0f, rim);
	renderer->DrawCircle(badgeX, badgeY, 18.5f, backing);
	renderer->DrawLabel("Lv", 10.0f, badgeX - 7.0f, badgeY + 16.0f, dim);
	const std::string levelText = std::to_string(m_Combat.level);
	const float levelWidth = MeasureDigits(renderer, levelText, 17.0f);
	DrawDigits(renderer, levelText, 17.0f, badgeX - levelWidth * 0.5f, badgeY + 7.5f, text);

	// ── 체력 ──
	const float barLeft = left + 56.0f;
	const float barWidth = 200.0f;
	const float barY = top - 10.0f;

	// 하트
	const Color heart = Tint(kHealRed, 1.15f);
	const float heartX = barLeft - 3.0f;
	renderer->DrawCircle(heartX - 2.7f, barY + 1.6f, 3.4f, heart);
	renderer->DrawCircle(heartX + 2.7f, barY + 1.6f, 3.4f, heart);
	renderer->DrawTriangle(heartX, barY - 2.4f, 12.4f, 7.8f, heart, 3.14159f);

	const float trackLeft = barLeft + 10.0f;
	renderer->DrawRoundRect(trackLeft + barWidth * 0.5f, barY, barWidth + 4.0f, 16.0f, 8.0f, backing);

	const float shownRatio = std::max(0.0f, m_Combat.healthShown / maxHealth);
	if (shownRatio > ratio)
	{
		const Color lag = { 0.95f, 0.85f, 0.75f, 0.55f };
		renderer->DrawRoundRect(trackLeft + barWidth * shownRatio * 0.5f, barY, barWidth * shownRatio, 11.0f, 5.5f, lag);
	}
	if (ratio > 0.0f)
	{
		const float pulse = (ratio < 0.3f) ? 0.75f + 0.25f * std::sin(m_Time * 8.0f) : 1.0f;
		const Color fill = { 0.78f * pulse, 0.18f * pulse, 0.16f * pulse, 1.0f };
		renderer->DrawRoundRect(trackLeft + barWidth * ratio * 0.5f, barY, barWidth * ratio, 11.0f, 5.5f, fill);
	}

	const std::string healthText = std::to_string(static_cast<int>(std::ceil(m_Combat.health)))
	                             + "/" + std::to_string(static_cast<int>(maxHealth));
	const float healthWidth = MeasureDigits(renderer, healthText, 12.0f);
	const Color outline = { 0.0f, 0.0f, 0.0f, 0.8f };
	DrawDigits(renderer, healthText, 12.0f, trackLeft + barWidth * 0.5f - healthWidth * 0.5f + 1.0f, barY + 7.0f, outline);
	DrawDigits(renderer, healthText, 12.0f, trackLeft + barWidth * 0.5f - healthWidth * 0.5f, barY + 8.0f, text);

	// ── 영혼(경험치) ──
	const float soulY = barY - 15.0f;
	const float needed = static_cast<float>(Progression::SoulsToNextLevel(m_Combat.level));
	const float soulRatio = std::min(1.0f, m_Combat.souls / needed);
	renderer->DrawRoundRect(barLeft - 3.0f, soulY, 6.0f, 6.0f, 0.8f, Tint(kSoulColor), 0.7854f);
	renderer->DrawRoundRect(trackLeft + barWidth * 0.5f, soulY, barWidth + 4.0f, 8.0f, 4.0f, backing);
	if (soulRatio > 0.0f)
	{
		renderer->DrawRoundRect(trackLeft + barWidth * soulRatio * 0.5f, soulY, barWidth * soulRatio, 4.0f, 2.0f, Tint(kSoulColor));
	}
	const std::string soulText = std::to_string(m_Combat.souls) + "/" + std::to_string(static_cast<int>(needed));
	DrawDigits(renderer, soulText, 11.0f, trackLeft + barWidth + 8.0f, soulY + 7.5f, dim);

	// ── 무기 강화와 자석 ──
	const float rowY = soulY - 17.0f;
	renderer->DrawSegment(barLeft - 7.0f, rowY - 4.0f, barLeft + 1.0f, rowY + 4.0f, 2.2f, Tint(kSteel));
	renderer->DrawSegment(barLeft - 7.5f, rowY + 0.5f, barLeft - 2.5f, rowY - 4.5f, 1.8f, Tint(kGold));
	for (int i = 0; i < Progression::kMaxWeaponTier; ++i)
	{
		const bool earned = i < m_Combat.weaponTier;
		renderer->DrawRoundRect(trackLeft + 4.0f + i * 13.0f, rowY, 9.0f, 5.0f, 2.0f,
		                        earned ? Tint(kGold) : Tint(kStoneDark, 1.2f));
	}

	if (m_Combat.magnetTime > 0.0f)
	{
		const float magnetX = trackLeft + 86.0f;
		const Color red = Tint(kMagnetRed, 1.2f);
		renderer->DrawRoundRect(magnetX, rowY - 3.0f, 9.0f, 3.2f, 1.6f, red);
		renderer->DrawSegment(magnetX - 3.2f, rowY - 3.0f, magnetX - 3.2f, rowY + 3.5f, 2.6f, red);
		renderer->DrawSegment(magnetX + 3.2f, rowY - 3.0f, magnetX + 3.2f, rowY + 3.5f, 2.6f, red);

		const float remain = m_Combat.magnetTime / 10.0f;
		renderer->DrawRoundRect(magnetX + 12.0f + 40.0f * remain * 0.5f, rowY, 40.0f * remain, 4.0f, 2.0f, Tint(kSoulColor));
	}

	// ── 종탑지기와 싸우는 중이면 화면 위쪽에 큰 체력 막대 ──
	for (const Enemy& enemy : m_Enemies)
	{
		if (enemy.kind != EnemyKind::Warden || !enemy.engaged || enemy.dying > 0.0f)
		{
			continue;
		}

		const float wardenRatio = std::max(0.0f, enemy.health / enemy.maxHealth);
		const float wardenWidth = 360.0f;
		const float wardenY = halfHeight - 44.0f;

		const float nameWidth = renderer->MeasureLabel("종탑지기", 15.0f);
		renderer->DrawLabel("종탑지기", 15.0f, -nameWidth * 0.5f, wardenY + 26.0f, Tint(kEyeRed, 1.0f));
		renderer->DrawRoundRect(0.0f, wardenY, wardenWidth + 6.0f, 12.0f, 6.0f, rim);
		renderer->DrawRoundRect(0.0f, wardenY, wardenWidth + 2.0f, 9.0f, 4.5f, backing);
		if (wardenRatio > 0.0f)
		{
			const Color fill = { 0.55f, 0.10f, 0.22f, 1.0f };
			renderer->DrawRoundRect(-wardenWidth * 0.5f + wardenWidth * wardenRatio * 0.5f, wardenY,
			                        wardenWidth * wardenRatio, 6.0f, 3.0f, fill);
		}
		break;
	}

	// ── 쓰러졌을 때 ──
	if (m_PendingRespawn)
	{
		const float dark = std::min(1.0f, (1.4f - m_RespawnTimer) / 0.6f);
		const Color veil = { 0.0f, 0.0f, 0.0f, 0.6f * dark };
		renderer->DrawRoundRect(0.0f, 0.0f, static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight), 0.0f, veil);
		const float width = renderer->MeasureLabel("쓰러졌다", 30.0f);
		renderer->DrawLabel("쓰러졌다", 30.0f, -width * 0.5f, 26.0f, Tint(kEyeRed, 1.0f, dark));
	}
}

