#include "stdafx.h"
#include "Actors.h"

#include "Art.h"
#include "Game.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

using namespace Art;

//
// 지형에서 나온 액터들.
//
// 땅, 집, 나무, 떨어진 물건, 종탑, 굳은 사람, 화톳불.
// 모두 제자리에 가만히 있고, 세계가 밝아지는 만큼 모습을 드러낸다.
//

// ═══ 땅 ═══════════════════════════════════════════════════════════
// 타일 수천 칸을 액터 하나가 그린다. 칸마다 액터를 두면 장면 그래프가
// 지형 때문에 터져 버리고, 땅은 서로 앞뒤를 가릴 일도 없기 때문이다.
GroundActor::GroundActor()
	: Actor("Ground")
{
	SetLayer(Layer::Ground);
}

void GroundActor::OnDraw(const DrawContext& context) const
{
	const Game* game = context.game;
	Renderer* renderer = context.renderer;
	const World& world = game->GetWorld();

	const float halfWidth = context.windowWidth * 0.5f;
	const float halfHeight = context.windowHeight * 0.5f;
	const float worldLeft = context.cameraX - halfWidth;
	const float worldRight = context.cameraX + halfWidth;
	const float worldTop = context.cameraY - halfHeight;
	const float worldBottom = context.cameraY + halfHeight;

	const int firstTileX = FloorToInt(worldLeft / kTileSize);
	const int lastTileX  = FloorToInt(worldRight / kTileSize);
	const int firstTileY = FloorToInt(worldTop / kTileSize);
	const int lastTileY  = FloorToInt(worldBottom / kTileSize);

	// ── 안개 층 ──
	// 배경색 자체가 연기다. 땅을 아주 어둡게 깔아 길과 광장의 윤곽만 남긴다.
	// 되찾은 구역 안에서는 이 하한이 종탑의 밝기까지 올라간다.
	for (int tileY = firstTileY; tileY <= lastTileY; ++tileY)
	{
		for (int tileX = firstTileX; tileX <= lastTileX; ++tileX)
		{
			const float worldX = TileToWorld(tileX);
			const float worldY = TileToWorld(tileY);
			const float brightness = std::max(kFogBrightness, game->BeaconBrightness(worldX, worldY));

			float ground[3];
			GroundColor(world.TileAt(tileX, tileY), tileX, tileY, ground);

			renderer->DrawSolidRect(context.ToRenderX(worldX), context.ToRenderY(worldY),
			                        0.0f, static_cast<float>(kTileSize),
			                        ground[0] * brightness,
			                        ground[1] * brightness,
			                        ground[2] * brightness,
			                        1.0f);
		}
	}

	// ── 조명 층 ──
	// 안개나 종탑 불빛보다 밝은 칸만 다시 그린다.
	const LightGrid& light = game->GetLight();
	const int firstCellX = light.CellOf(worldLeft);
	const int lastCellX  = light.CellOf(worldRight);
	const int firstCellY = light.CellOf(worldTop);
	const int lastCellY  = light.CellOf(worldBottom);

	for (int cellY = firstCellY; cellY <= lastCellY; ++cellY)
	{
		for (int cellX = firstCellX; cellX <= lastCellX; ++cellX)
		{
			const float dynamic = light.Brightness(cellX, cellY);
			if (dynamic < kFogBrightness)
			{
				continue;
			}

			const float worldX = (cellX + 0.5f) * kLightCell;
			const float worldY = (cellY + 0.5f) * kLightCell;

			const float floorLevel = std::max(kFogBrightness, game->BeaconBrightness(worldX, worldY));
			if (dynamic <= floorLevel + 0.01f)
			{
				continue;
			}

			const int tileX = FloorToInt(worldX / kTileSize);
			const int tileY = FloorToInt(worldY / kTileSize);
			float ground[3];
			GroundColor(world.TileAt(tileX, tileY), tileX, tileY, ground);

			// 불이 직접 비추는 곳은 따뜻하게, 남은 발자국은 차갑게 식어간다.
			const float warmth = light.Glow(cellX, cellY) * 0.35f;

			renderer->DrawSolidRect(
				context.ToRenderX(worldX), context.ToRenderY(worldY), 0.0f, kLightCell,
				Mix(ground[0], kFlame[0], warmth) * dynamic,
				Mix(ground[1], kFlame[1], warmth) * dynamic,
				Mix(ground[2], kFlame[2], warmth) * dynamic,
				1.0f);
		}
	}
}

// ═══ 집 ═══════════════════════════════════════════════════════════
HouseActor::HouseActor(const World::House& house)
	: Actor("House")
	, m_House(house)
{
	// 자리는 앞벽의 아래 모서리. 집은 거기서 위로 자란다.
	SetPosition(static_cast<float>(house.tileX * kTileSize),
	            static_cast<float>((house.tileY + house.depth) * kTileSize));
}

void HouseActor::OnDraw(const DrawContext& context) const
{
	const float left = WorldX();
	const float bottom = WorldY();
	const float frontX = left + m_House.width * kTileSize * 0.5f;
	const float frontY = bottom - kTileSize * 0.5f;

	// 되찾은 마을의 집에는 사람이 돌아와 창에 불이 켜진다.
	const bool lived = context.game->BeaconBrightness(frontX, frontY) > 0.0f;

	DrawHouse(context.renderer, context.ToRenderX(left), context.ToRenderY(bottom), m_House,
	          context.game->StructureShade(frontX, frontY),
	          context.game->GlowAt(frontX, frontY) * 0.3f, lived, context.time);
}

// ═══ 나무 ═════════════════════════════════════════════════════════
TreeActor::TreeActor(const World::Tree& tree)
	: Actor("Tree")
	, m_Tree(tree)
{
	// 밑동이 칸 한가운데보다 조금 아래에 있어야 앞뒤 정렬이 자연스럽다.
	SetPosition(TileToWorld(tree.tileX), TileToWorld(tree.tileY) + 6.0f);
}

void TreeActor::OnDraw(const DrawContext& context) const
{
	DrawTree(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()), m_Tree,
	         context.game->StructureShade(WorldX(), WorldY()),
	         context.game->GlowAt(WorldX(), WorldY()) * 0.3f, context.time);
}

// ═══ 떨어져 있는 물건 ═════════════════════════════════════════════
PropActor::PropActor(const World::Prop& prop)
	: Actor("Prop")
	, m_Prop(prop)
{
	SetPosition(TileToWorld(prop.tileX), TileToWorld(prop.tileY));
}

void PropActor::OnDraw(const DrawContext& context) const
{
	// 사물은 사람과 달리 안개 속에서 보이지 않는다. 불을 가져가야 드러난다.
	const float brightness = context.game->TotalBrightness(WorldX(), WorldY());
	if (brightness < kRevealAt)
	{
		return;
	}

	// 밝기 문턱을 막 넘긴 것은 희미하게, 충분히 밝으면 또렷하게.
	const float reveal = std::min(1.0f, (brightness - kRevealAt) / 0.35f);
	DrawProp(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	         m_Prop, std::max(0.0f, reveal));
}

// ═══ 종탑 ═════════════════════════════════════════════════════════
TowerActor::TowerActor(int regionX, int regionY, float worldX, float worldY)
	: Actor("Tower")
	, m_RegionX(regionX)
	, m_RegionY(regionY)
{
	SetPosition(worldX, worldY);
}

void TowerActor::OnDraw(const DrawContext& context) const
{
	// 구역의 상태는 Regions가 들고 있다. 울렸는지 매 프레임 물어본다.
	const Regions::Region region = context.game->GetRegions().At(m_RegionX, m_RegionY);
	DrawTower(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()), region,
	          context.game->RingProgress(), context.game->IsRingingAt(m_RegionX, m_RegionY),
	          context.time);
}

// ═══ 굳어 있는 사람 ═══════════════════════════════════════════════
SleeperActor::SleeperActor(int regionX, int regionY, float worldX, float worldY)
	: Actor("Sleeper")
	, m_RegionX(regionX)
	, m_RegionY(regionY)
{
	SetPosition(worldX, worldY);
}

void SleeperActor::OnUpdate(const ActorContext& context)
{
	Game* game = context.game;
	Regions::Region region = game->GetRegions().At(m_RegionX, m_RegionY);

	// 따라나섰으면 이 자리에는 더 이상 아무도 없다.
	if (region.woken)
	{
		Destroy();
		return;
	}

	// 불을 비추고 있는 동안 조금씩 생기를 되찾는다. 등불은 내가 선 자리만
	// 밝히므로, 자리를 뜨면 다시 식기 시작한다. 그래서 데려가려면 말을 걸어
	// 따라오게 하는 수밖에 없다.
	const float brightness = game->TotalBrightness(WorldX(), WorldY());
	float warmth = region.warmth;
	warmth += (brightness >= kRevealAt)
		? context.deltaSeconds / kReviveSeconds
		: -context.deltaSeconds / kChillSeconds;
	warmth = std::max(0.0f, std::min(1.0f, warmth));

	if (warmth != region.warmth)
	{
		game->GetRegions().SetWarmth(m_RegionX, m_RegionY, warmth);
	}
}

void SleeperActor::OnDraw(const DrawContext& context) const
{
	const Regions::Region region = context.game->GetRegions().At(m_RegionX, m_RegionY);
	const float facing = (context.game->PlayerX() >= WorldX()) ? 1.0f : -1.0f;

	// 말을 거는 동안에는 생기와 상관없이 일어서서 마주 본다.
	DrawSleeper(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	            region.warmth, context.game->IsTalkingWith(m_RegionX, m_RegionY), facing, context.time);
}

// ═══ 화톳불 ═══════════════════════════════════════════════════════
FireActor::FireActor(float worldX, float worldY, float radius)
	: Actor("Bonfire")
	, m_Radius(radius)
	, m_Seed(worldX * 0.013f + worldY * 0.021f)
{
	SetPosition(worldX, worldY);
}

void FireActor::OnDraw(const DrawContext& context) const
{
	DrawBonfire(context.renderer, context.ToRenderX(WorldX()), context.ToRenderY(WorldY()),
	            context.time, m_Seed);
}

bool FireActor::GetLight(float& radius, bool& leavesTrail) const
{
	// 화톳불은 그 자리에 계속 있으므로 발자국을 남길 필요가 없다.
	radius = m_Radius;
	leavesTrail = false;
	return true;
}
