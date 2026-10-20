#pragma once

#include "Combat.h"
#include "Regions.h"
#include "Renderer.h"
#include "World.h"

//
// 그림 도구 상자.
//
// 팔레트, 세계의 기준값, 그리고 사람 · 집 · 나무 · 불처럼 되풀이해 그리는
// 도안을 모아 둔다. 액터들이 모두 여기서 꺼내 쓴다.
//
// 좌표는 화면 중앙이 원점이고 y축이 위쪽인 렌더러 좌표계다 — 월드 좌표가 아니다.
// 대부분의 도안에서 (x, y)는 그 물체의 발밑이다.
//
namespace Art
{
	// 사람 하나를 그리는 법. 자세(posture) 하나로 굳어 웅크린 모습과
	// 바로 선 모습을 오간다.
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

	// ── 세계의 기준값과 팔레트 ──
	extern const int kTileSize;
	extern const float kLightCell;
	extern const float kFogBrightness;
	extern const float kRevealAt;
	extern const float kLanternRadius;
	extern const float kOilSeconds;
	extern const float kFireRadius;
	extern const float kReviveSeconds;
	extern const float kChillSeconds;
	extern const float kRingSeconds;
	extern const float kTowerReach;
	extern const float kCompanionReach;
	extern const float kWakeReach;
	extern const float kBeaconBrightness;
	extern const float kFollowLag;
	extern const float kPathKeep;
	extern const float kPathStep;
	extern const float kGrassColor[3];
	extern const float kPathColor[3];
	extern const float kPlazaColor[3];
	extern const float kCanopy[3];        // 깊은 숲의 수관
	extern const float kCanopyLight[3];
	extern const float kFlame[3];
	extern const float kRubble[3];
	extern const float kSupply[3];
	extern const float kStone[3];
	extern const float kCrow[3];
	extern const float kLiving[3];
	extern const float kFrozen[3];
	extern const float kGlowLantern;
	extern const float kGlowFire;
	extern const float kGlowBell;
	extern const float kGlowSpark;
	extern const float kGlowGauge;
	extern const float kGlowWindow;
	extern const float kCloak[3];
	extern const float kHood[3];
	extern const float kTunic[3];
	extern const float kHair[3];
	extern const float kMetal[3];
	extern const float kWood[3];
	extern const float kWoodDark[3];
	extern const float kEmber[3];
	extern const float kFlameCore[3];
	extern const float kStoneDark[3];
	extern const float kRoof[3];
	extern const float kBronze[3];
	extern const float kRope[3];
	extern const float kPlaster[3];
	extern const float kTimber[3];
	extern const float kRoofTile[3];
	extern const float kRoofSlate[3];
	extern const float kThatch[3];
	extern const float kWindowDark[3];
	extern const float kLeaf[3];
	extern const float kLeafLight[3];
	extern const float kLeafDry[3];
	extern const float kPine[3];
	extern const float kBark[3];
	extern const float kDeadWood[3];
	extern const float kMapGrass[3];
	extern const float kMapPath[3];
	extern const float kMapPlaza[3];
	extern const float kMapHouse[3];
	extern const float kMapTree[3];
	extern const float kMapFrozen[3];
	extern const float kMapTower[3];
	extern const float kMapRubble[3];
	extern const float kMapSupply[3];
	extern const float kMapBarrel[3];
	extern const int kMapTiles;
	extern const float kMapPixelsPerTile;
	extern const float kMapViewSize;
	extern const float kShadeBody[3];
	extern const float kShadeSkin[3];
	extern const float kWardenRobe[3];
	extern const float kRust[3];
	extern const float kEyeRed[3];
	extern const float kEyePale[3];
	extern const float kSoulColor[3];
	extern const float kHealRed[3];
	extern const float kSteel[3];
	extern const float kGold[3];
	extern const float kMagnetRed[3];
	extern const float kOrbCore[3];
	extern const float kOrbRim[3];
	extern const float kGlowEye;
	extern const float kGlowEmber;
	extern const float kGlowSoul;
	extern const int kMarkTowerSilent;
	extern const int kMarkTowerRung;
	extern const int kMarkSleeper;
	extern const int kMarkCompanion;
	extern const int kMarkFire;
	extern const int kMarkSupply;
	extern const int kMarkBarrel;
	extern const int kMarkRubble;
	extern const int kMarkWarden;
	extern const int kMarkEnemy;

	// ── 셈 도우미 ──
	float Mix(float a, float b, float t);
	int FloorToInt(float value);
	float Distance(float ax, float ay, float bx, float by);
	float TileToWorld(int tile);
	// 종탑 불빛은 울타리 모양의 사각형이다. 거리는 가로·세로 중 먼 쪽(체비쇼프)으로 잰다.
	//   BeaconInner  — 여기까지는 한껏 밝다(울타리 바로 안쪽)
	//   BeaconRadius — 여기서 꺼진다(문 앞 네 칸). 어둠은 이 너머에서 멈춘다
	float BeaconRadius();
	float BeaconInner();
	float VillageWallDistance();   // 종탑에서 울타리까지
	float VillageLane();           // 종탑에서 울타리 안쪽 안길까지
	unsigned int PropHash(int x, int y);
	unsigned char ToByte(float value);

	// ── 색 ──
	Color Tint(const float c[3], float shade = 1.0f, float alpha = 1.0f);
	Color Blend(const float a[3], const float b[3], float t, float shade = 1.0f);
	Color LitTint(const float c[3], float shade, float warmth);
	void GroundColor(unsigned char tile, int tileX, int tileY, float out[3]);

	// ── 도안 ──
	void DrawShadow(Renderer* r, float x, float y, float radiusX, float alpha = 0.38f);
	void DrawFigure(Renderer* r, float x, float y, const FigureLook& look,
	                float* outHandX = NULL, float* outHandY = NULL);
	void DrawLantern(Renderer* r, float handX, float handY, float swing, float glow);
	void DrawBonfire(Renderer* r, float x, float y, float time, float seed);
	void DrawCrow(Renderer* r, float x, float y, float facing, float time, float seed);
	void DrawProp(Renderer* r, float x, float y, const World::Prop& prop, float shade);
	void DrawHouse(Renderer* r, float left, float bottom, const World::House& house,
	               float shade, float warmth, bool lived, float time);
	void DrawTree(Renderer* r, float x, float y, const World::Tree& tree,
	              float shade, float warmth, float time);

	// 울타리 한 칸. (x, y)는 칸 한가운데의 땅. lit이면 문 기둥에 불이 켜진다.
	void DrawWall(Renderer* r, float x, float y, const World::Wall& wall,
	              float shade, float warmth, bool lit, float time);

	// 상인에게 사서 세운 횃불
	void DrawTorch(Renderer* r, float x, float y, float time, float seed);

	// 마을 사람이 집 둘레에 놓은 장식. tint는 그 사람의 빛깔
	void DrawDecor(Renderer* r, float x, float y, Decor kind, const float tint[3],
	               float shade, float warmth, float time, unsigned int seed);

	// 깊은 숲 한 칸의 수관. 나무 액터 대신 땅이 그린다.
	void DrawCanopy(Renderer* r, float x, float y, int tileX, int tileY, float shade, float warmth);
	void DrawMapMarker(Renderer* r, int marker, float x, float y, float warmth);

	// ── 놓인 것들 ──
	void DrawTower(Renderer* r, float x, float y, const Regions::Region& region,
	               float ringProgress, bool ringing, float time);
	void DrawSleeper(Renderer* r, float x, float y, float warmth, bool standing, float facing, float time);
	void DrawCompanion(Renderer* r, float x, float y, float alive, float facing, bool walking, float time);
	void DrawPlayer(Renderer* r, float x, float y, float oil, float facing, bool moving, float time);
	void DrawEnemy(Renderer* r, float x, float y, const Enemy& enemy, float facing, float time);
	void DrawPickup(Renderer* r, float x, float y, const Pickup& pickup, float seed, float time);
	void DrawEmber(Renderer* r, float x, float y, float velocityX, float velocityY);
	void DrawOrb(Renderer* r, float x, float y, float seed, float time);
}
