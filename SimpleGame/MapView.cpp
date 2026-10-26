#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Input.h"
#include "Renderer.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace Art;

//
// 지도 — 오른쪽 위의 미니맵과 M으로 여는 큰 지도.
//
// 둘은 같은 그림을 쓴다. 구역(272x272칸)마다 그림 한 장을 두고, 밝혀 본 칸은
// 그 땅의 색으로, 못 본 칸은 투명하게 둔다. 그 뒤에 검은 바탕을 깔면 안 밝힌
// 곳은 검게 남는다.
//
//   · 칸을 처음 밝히는 순간 그 한 칸만 칠한다(MarkSeen). 지도를 다시 그리려고
//     지형을 훑는 일이 없다.
//   · 칠한 그림은 0.15초에 한 번 몰아서 올린다. 걸으며 매 프레임 밝혀도 올리는
//     횟수는 일정하다.
//   · 종지기가 있는 마을(첫 마을, 종을 울린 마을)은 울타리와 문 앞까지 통째로
//     밝힌다(RevealVillage). 흑백 그림의 사각형을 흰색으로 칠하는 일이다.
//
// 미니맵은 내 둘레를 작게, 큰 지도는 끌어서 옮기고 굴려서 확대해 어디든 본다.
//
namespace
{
	long long SheetKey(int regionX, int regionY)
	{
		return (static_cast<long long>(regionY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(regionX));
	}

	const float kSheetUploadEvery = 0.15f;

	// 큰 지도의 확대 범위. 월드 1px이 화면 몇 px인가.
	const float kBigMapMinScale = 0.008f;   // 마을 예닐곱이 한 화면에
	const float kBigMapMaxScale = 0.6f;     // 한 칸이 거의 20px

	// 마을 이름표. "3번째 마을"처럼 바뀌지 않는 글이라 텍스처는 마을 수만큼만 생긴다.
	std::string VillageLabel(int index)
	{
		return (index == 0) ? std::string("첫 마을") : std::to_string(index + 1) + "번째 마을";
	}
}

// ═══ 칠하기 ═══════════════════════════════════════════════════════

void Game::MarkSeen(int tileX, int tileY)
{
	if (m_Explored.Mark(tileX, tileY))
	{
		PaintMapTile(tileX, tileY);
	}
}

void Game::PaintMapTile(int tileX, int tileY)
{
	const int span = ExploredMap::Span();
	MapSheet& sheet = m_MapSheets[SheetKey(Regions::RegionOfTile(tileX), Regions::RegionOfTile(tileY))];
	if (sheet.pixels.empty())
	{
		sheet.pixels.assign(static_cast<size_t>(span) * span * 4, 0);
	}

	const unsigned char tile = m_World.TileAt(tileX, tileY);
	const float* color = kMapGrass;
	if (tile == World::kPath)       { color = kMapPath; }
	else if (tile == World::kPlaza) { color = kMapPlaza; }
	else if (tile == World::kHouse) { color = kMapHouse; }
	else if (tile == World::kTree)  { color = kMapTree; }
	else if (tile == World::kWall)  { color = kMapPlaza; }   // 울타리는 밝은 돌빛 선으로
	else if (tile == World::kWoods) { color = kMapTree; }

	const float jitter = static_cast<float>((tileX * 31 + tileY * 17) & 3) * 0.012f;
	unsigned char* pixel = &sheet.pixels[(static_cast<size_t>(World::PositiveMod(tileY, span)) * span
	                                      + World::PositiveMod(tileX, span)) * 4];
	pixel[0] = ToByte(color[0] + jitter);
	pixel[1] = ToByte(color[1] + jitter);
	pixel[2] = ToByte(color[2] + jitter);
	pixel[3] = 255;
	sheet.dirty = true;
}

void Game::RevealVillage(int village)
{
	if (village < 0)
	{
		return;
	}
	int regionX = 0;
	int regionY = 0;
	m_Regions.VillagePosition(village, regionX, regionY);

	// 울타리와 그 바깥 네 칸(종소리가 닿는 곳)까지. 문 앞 숲길 어귀가 함께 보인다.
	const int span = Regions::RegionTileSpan();
	const int reach = 4;
	const int low = World::kWallLow - reach;
	const int high = World::kWallHigh + reach;
	for (int localY = low; localY <= high; ++localY)
	{
		for (int localX = low; localX <= high; ++localX)
		{
			MarkSeen(regionX * span + localX, regionY * span + localY);
		}
	}
}

// ═══ 그림 깔기 ════════════════════════════════════════════════════

void Game::DrawMapSheets(Renderer* renderer, float worldCenterX, float worldCenterY, float scale,
                         float screenCenterX, float screenCenterY, float halfWidth, float halfHeight)
{
	const int span = ExploredMap::Span();
	const float sheetWorld = static_cast<float>(span * kTileSize);
	const float size = sheetWorld * scale;
	const bool uploadNow = m_MapUploadAge >= kSheetUploadEvery;
	bool uploaded = false;

	for (auto& entry : m_MapSheets)
	{
		const int regionY = static_cast<int>(entry.first >> 32);
		const int regionX = static_cast<int>(static_cast<unsigned int>(entry.first & 0xFFFFFFFFll));

		const float left = screenCenterX + (static_cast<float>(regionX) * sheetWorld - worldCenterX) * scale;
		const float top = screenCenterY - (static_cast<float>(regionY) * sheetWorld - worldCenterY) * scale;
		if (left > screenCenterX + halfWidth || left + size < screenCenterX - halfWidth
		    || top < screenCenterY - halfHeight || top - size > screenCenterY + halfHeight)
		{
			continue;   // 화면 밖
		}

		MapSheet& sheet = entry.second;
		if (sheet.image == 0)
		{
			sheet.image = renderer->CreateImage(span, span);
			renderer->UpdateImage(sheet.image, span, span, sheet.pixels.data());
			sheet.dirty = false;
		}
		else if (sheet.dirty && uploadNow)
		{
			renderer->UpdateImage(sheet.image, span, span, sheet.pixels.data());
			sheet.dirty = false;
			uploaded = true;
		}

		renderer->DrawImage(sheet.image, left, top, size, size);
	}

	if (uploaded)
	{
		m_MapUploadAge = 0.0f;
	}
}

// ═══ 미니맵 ═══════════════════════════════════════════════════════

void Game::DrawMinimap(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Minimap);
	const int playerTileX = FloorToInt(m_PlayerX / kTileSize);
	const int playerTileY = FloorToInt(m_PlayerY / kTileSize);

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

	// ── 밝혀 본 땅 ── 플레이어가 움직이면 그림이 부드럽게 흘러간다
	DrawMapSheets(renderer, m_PlayerX, m_PlayerY, scale, centerX, centerY, half, half);

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
		// 굳은 사람은 본 적이 있거나 정보상에게 들었을 때만
		if (!region.woken && (region.heard || m_Explored.IsSeen(region.sleeperTileX, region.sleeperTileY)))
		{
			DrawMapMarker(renderer, kMarkSleeper,
			              mapX(TileToWorld(region.sleeperTileX)), mapY(TileToWorld(region.sleeperTileY)),
			              region.warmth);
		}

		// 종탑은 본 적이 있거나, 그 마을 사람에게서 위치를 들었으면 표시한다.
		if (region.rung || region.woken || region.heard
		    || m_Explored.IsSeen(region.towerTileX, region.towerTileY))
		{
			DrawMapMarker(renderer, region.rung ? kMarkTowerRung : kMarkTowerSilent,
			              mapX(TileToWorld(region.towerTileX)), mapY(TileToWorld(region.towerTileY)), 0.0f);
		}
	}

	// 내려놓은 화톳불. 장면에서 직접 찾는다 — 따로 목록을 들고 있지 않다.
	m_Scene.ForEach([&](Actor& actor)
	{
		if (actor.Name() == "Bonfire" || actor.Name() == "Torch")
		{
			DrawMapMarker(renderer, kMarkFire, mapX(actor.WorldX()), mapY(actor.WorldY()), 0.0f);
		}
	});

	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		if (const VillagerActor* member = FindVillager(m_Party[i]))
		{
			DrawMapMarker(renderer, kMarkParty, mapX(member->WorldX()), mapY(member->WorldY()), 0.0f);
		}
	}

	if (HasCompanion())
	{
		DrawMapMarker(renderer, kMarkCompanion, mapX(m_Companion->WorldX()), mapY(m_Companion->WorldY()), 0.0f);
	}

	// 적. 종탑지기는 따로 크게 표시한다.
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		const EnemyActor* enemy = m_Enemies[i];
		if (enemy->IsDying())
		{
			continue;
		}
		DrawMapMarker(renderer, enemy->IsWarden() ? kMarkWarden : kMarkEnemy,
		              mapX(enemy->WorldX()), mapY(enemy->WorldY()), 0.0f);
	}

	// 나 — 바라보는 쪽을 가리키는 화살표
	const Color outline = { 0.01f, 0.01f, 0.02f, 0.95f };
	const Color me = { 0.97f, 0.92f, 0.80f, 1.0f };
	const float pointing = -1.5708f * m_Player->Facing();
	renderer->DrawTriangle(centerX, centerY, 12.0f, 12.0f, outline, pointing);
	renderer->DrawTriangle(centerX, centerY, 8.0f, 8.5f, me, pointing);

	renderer->ClearClip();

	// ── 범례 ──
	const int kLegend = 11;
	const int legendRows = (kLegend + 1) / 2;
	const float legendTop = centerY - half - 20.0f;
	const float rowHeight = 17.0f;
	const Color legendBacking = { 0.015f, 0.017f, 0.025f, 0.72f };
	const Color legendText = { 0.78f, 0.76f, 0.71f, 1.0f };
	renderer->DrawRoundRect(centerX, legendTop - rowHeight * (legendRows - 1) * 0.5f, kMapViewSize + 8.0f,
	                        rowHeight * legendRows + 10.0f, 6.0f, legendBacking);

	const int markers[kLegend] = { kMarkTowerSilent, kMarkTowerRung, kMarkSleeper, kMarkCompanion,
	                               kMarkParty, kMarkFire, kMarkSupply, kMarkBarrel, kMarkRubble,
	                               kMarkWarden, kMarkEnemy };
	const char* const labels[kLegend] = { "종탑", "울린 종탑", "굳은 사람", "동행자",
	                                      "파티", "화톳불", "보급품", "나무통", "잔해",
	                                      "종탑지기", "적" };

	for (int i = 0; i < kLegend; ++i)
	{
		const float itemX = centerX - half + 10.0f + (i % 2) * half;
		const float itemY = legendTop - (i / 2) * rowHeight;
		DrawMapMarker(renderer, markers[i], itemX, itemY, 0.0f);
		renderer->DrawLabel(labels[i], 12.0f, itemX + 11.0f, itemY + 8.0f, legendText);
	}

	// 범례가 홀수라 마지막 줄 오른쪽이 빈다. 거기에 큰 지도 여는 키를 적는다.
	const std::string hint = "M  큰 지도";
	renderer->DrawLabel(hint, 11.0f, centerX + half - 6.0f - renderer->MeasureLabel(hint, 11.0f),
	                    legendTop - (legendRows - 1) * rowHeight + 7.0f, Tint(kStone, 1.5f));
}

// ═══ 큰 지도 ══════════════════════════════════════════════════════
//
// 화면을 다 덮는다. 열려 있는 동안 세계는 멈춘다.
//   · 끌기 — 옮긴다
//   · 휠 — 마우스가 가리키는 곳을 축으로 확대 · 축소
//   · + / - — 화면 가운데를 축으로 확대 · 축소
//   · Space — 내가 선 곳으로
//   · M · ESC — 닫는다
//
void Game::UpdateBigMap()
{
	auto zoomAt = [&](float factor, float screenX, float screenY)
	{
		// 가리킨 곳의 월드 좌표가 확대한 뒤에도 같은 화면 자리에 오도록
		const float worldX = m_BigMap.centerX + screenX / m_BigMap.scale;
		const float worldY = m_BigMap.centerY - screenY / m_BigMap.scale;
		m_BigMap.scale = std::max(kBigMapMinScale, std::min(kBigMapMaxScale, m_BigMap.scale * factor));
		m_BigMap.centerX = worldX - screenX / m_BigMap.scale;
		m_BigMap.centerY = worldY + screenY / m_BigMap.scale;
	};

	if (m_WheelDelta != 0.0f)
	{
		zoomAt(std::pow(1.18f, m_WheelDelta), m_MouseX, m_MouseY);
	}
	if (KeyPressed('=') || KeyPressed('+'))
	{
		zoomAt(1.5f, 0.0f, 0.0f);
	}
	if (KeyPressed('-') || KeyPressed('_'))
	{
		zoomAt(1.0f / 1.5f, 0.0f, 0.0f);
	}
	if (KeyPressed(' '))
	{
		m_BigMap.centerX = m_PlayerX;
		m_BigMap.centerY = m_PlayerY;
	}

	if (m_MousePressed[Input::kMouseLeft])
	{
		m_BigMap.dragging = true;
		m_BigMap.grabX = m_MouseX;
		m_BigMap.grabY = m_MouseY;
		m_BigMap.grabCenterX = m_BigMap.centerX;
		m_BigMap.grabCenterY = m_BigMap.centerY;
	}
	if (m_BigMap.dragging)
	{
		// 화면은 y가 위, 월드는 y가 아래다
		m_BigMap.centerX = m_BigMap.grabCenterX - (m_MouseX - m_BigMap.grabX) / m_BigMap.scale;
		m_BigMap.centerY = m_BigMap.grabCenterY + (m_MouseY - m_BigMap.grabY) / m_BigMap.scale;
		if (!m_MouseHeld[Input::kMouseLeft])
		{
			m_BigMap.dragging = false;
		}
	}
}

void Game::DrawBigMap(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Minimap);

	const float width = static_cast<float>(m_WindowWidth);
	const float height = static_cast<float>(m_WindowHeight);
	const float halfWidth = width * 0.5f - 14.0f;
	const float halfHeight = height * 0.5f - 14.0f;
	const float scale = m_BigMap.scale;

	auto toX = [&](float worldX) { return (worldX - m_BigMap.centerX) * scale; };
	auto toY = [&](float worldY) { return -(worldY - m_BigMap.centerY) * scale; };

	// ── 바탕 ── 안 밝힌 곳은 그대로 검다
	renderer->DrawRoundRect(0.0f, 0.0f, width, height, 0.0f, Color{ 0.0f, 0.0f, 0.0f, 0.82f });
	renderer->DrawRoundRect(0.0f, 0.0f, halfWidth * 2.0f + 4.0f, halfHeight * 2.0f + 4.0f, 8.0f,
	                        Color{ kBronze[0], kBronze[1], kBronze[2], 0.5f });
	renderer->DrawRoundRect(0.0f, 0.0f, halfWidth * 2.0f, halfHeight * 2.0f, 6.0f, Color{ 0.0f, 0.0f, 0.0f, 1.0f });

	renderer->SetClip(-halfWidth, -halfHeight, halfWidth * 2.0f, halfHeight * 2.0f);
	DrawMapSheets(renderer, m_BigMap.centerX, m_BigMap.centerY, scale, 0.0f, 0.0f, halfWidth, halfHeight);

	// ── 마을 ── 길을 따라 앞에서부터. 아는 마을(밝혀 봤거나 소식을 들은 곳)만 적는다.
	// 길은 끝없이 이어지므로, 모르는 마을이 셋 이어지면 그만 본다.
	const Color nameLit = Tint(kFlame, 1.05f);
	const Color nameDark = { 0.72f, 0.74f, 0.80f, 1.0f };
	int unknownInRow = 0;
	for (int index = 0; index < 256 && unknownInRow < 3; ++index)
	{
		int regionX = 0;
		int regionY = 0;
		m_Regions.VillagePosition(index, regionX, regionY);
		const Regions::Region region = m_Regions.At(regionX, regionY);
		const bool known = region.rung || region.heard || m_Explored.Mask(regionX, regionY) != NULL;
		if (!known)
		{
			++unknownInRow;
			continue;
		}
		unknownInRow = 0;

		const float towerX = toX(TileToWorld(region.towerTileX));
		const float towerY = toY(TileToWorld(region.towerTileY));

		if (!region.woken && (region.heard || m_Explored.IsSeen(region.sleeperTileX, region.sleeperTileY)))
		{
			DrawMapMarker(renderer, kMarkSleeper, toX(TileToWorld(region.sleeperTileX)),
			              toY(TileToWorld(region.sleeperTileY)), region.warmth);
		}
		if (region.rung || region.woken || region.heard || m_Explored.IsSeen(region.towerTileX, region.towerTileY))
		{
			DrawMapMarker(renderer, region.rung ? kMarkTowerRung : kMarkTowerSilent, towerX, towerY, 0.0f);
		}

		const std::string label = VillageLabel(index);
		const float labelSize = 13.0f;
		const float labelWidth = renderer->MeasureLabel(label, labelSize);
		renderer->DrawRoundRect(towerX, towerY + 22.0f, labelWidth + 12.0f, 19.0f, 9.0f,
		                        Color{ 0.0f, 0.0f, 0.0f, 0.6f });
		renderer->DrawLabel(label, labelSize, towerX - labelWidth * 0.5f, towerY + 31.0f,
		                    region.rung ? nameLit : nameDark);
	}

	// ── 놓은 불, 함께 다니는 사람들, 나 ──
	m_Scene.ForEach([&](Actor& actor)
	{
		if (actor.Name() == "Bonfire" || actor.Name() == "Torch")
		{
			DrawMapMarker(renderer, kMarkFire, toX(actor.WorldX()), toY(actor.WorldY()), 0.0f);
		}
	});
	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		if (const VillagerActor* member = FindVillager(m_Party[i]))
		{
			DrawMapMarker(renderer, kMarkParty, toX(member->WorldX()), toY(member->WorldY()), 0.0f);
		}
	}
	if (HasCompanion())
	{
		DrawMapMarker(renderer, kMarkCompanion, toX(m_Companion->WorldX()), toY(m_Companion->WorldY()), 0.0f);
	}

	const Color outline = { 0.01f, 0.01f, 0.02f, 0.95f };
	const Color me = { 0.97f, 0.92f, 0.80f, 1.0f };
	const float pointing = -1.5708f * m_Player->Facing();
	const float meX = toX(m_PlayerX);
	const float meY = toY(m_PlayerY);
	renderer->DrawCircle(meX, meY, 11.0f, Color{ me.r, me.g, me.b, 0.18f + 0.1f * std::sin(m_Time * 4.0f) });
	renderer->DrawTriangle(meX, meY, 14.0f, 14.0f, outline, pointing);
	renderer->DrawTriangle(meX, meY, 9.5f, 10.0f, me, pointing);

	renderer->ClearClip();

	// ── 제목과 안내 ──
	const Color title = { 0.96f, 0.88f, 0.68f, 1.0f };
	const Color note = { 0.70f, 0.71f, 0.75f, 1.0f };
	const Color backing = { 0.0f, 0.0f, 0.0f, 0.65f };
	renderer->DrawRoundRect(-halfWidth + 50.0f, halfHeight - 20.0f, 76.0f, 26.0f, 8.0f, backing);
	renderer->DrawLabel("지도", 17.0f, -halfWidth + 26.0f, halfHeight - 9.0f, title);

	const std::string hint = "끌어서 옮기기   휠 · +/-  확대 · 축소   Space  내 자리   M  닫기";
	const float hintWidth = renderer->MeasureLabel(hint, 12.0f);
	renderer->DrawRoundRect(0.0f, -halfHeight + 18.0f, hintWidth + 24.0f, 24.0f, 10.0f, backing);
	renderer->DrawLabel(hint, 12.0f, -hintWidth * 0.5f, -halfHeight + 26.0f, note);
}
