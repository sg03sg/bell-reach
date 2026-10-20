#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Dialogue.h"
#include "Input.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

// Art의 팔레트와 도안을 이름 그대로 쓴다.
using namespace Art;

namespace
{
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


const Regions::Region* Game::FindTalkableSleeper() const
{
	if (HasCompanion())
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
	m_Player->SetMoving(false);
	m_Player->SetFacing((sleeperX >= m_PlayerX) ? 1.0f : -1.0f);
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
	m_Companion = static_cast<CompanionActor*>(m_Scene.Spawn(std::unique_ptr<Actor>(
		new CompanionActor(TileToWorld(region.sleeperTileX), TileToWorld(region.sleeperTileY),
		                   region.regionX, region.regionY))));
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

	if (HasCompanion() && !(m_Companion != NULL && m_Companion->IsFrozenAgain()))
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
			 && Distance(m_Companion->WorldX(), m_Companion->WorldY(), towerX, towerY) <= kCompanionReach)
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
		// 종탑 액터가 그 사람을 대신 그려 준다.
		if (m_Companion != NULL)
		{
			m_Companion->Destroy();
			m_Companion = NULL;
		}

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
	if (placeFire && !m_PrevPlaceFire && m_Player->FiresLeft() > 0)
	{
		m_Scene.Spawn(std::unique_ptr<Actor>(new FireActor(m_PlayerX, m_PlayerY, kFireRadius)));
		m_Player->UseFire();
	}

	m_PrevInteract = interact;
	m_PrevPlaceFire = placeFire;
}







void Game::DrawHud(Renderer* renderer)
{
	DrawCombatHud(renderer);

	// 글자를 그릴 수단이 아직 없으므로 모양으로 표시한다.
	// 기획서의 "UI 없이 색으로 읽히게" 방침과도 맞는다.
	const float left = -m_WindowWidth * 0.5f + 22.0f;
	const float bottom = -m_WindowHeight * 0.5f + 20.0f;

	// 등불 기름 — 기름방울 16개
	const int oilDrops = 16;
	const int oilFilled = static_cast<int>(Oil() * oilDrops + 0.5f);
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
		const float shade = (i < m_Player->FiresLeft()) ? 1.0f : 0.16f;
		const float fireX = left + 196.0f + i * 17.0f;
		renderer->DrawRoundRect(fireX, bottom - 5.0f, 11.0f, 2.6f, 1.2f, Tint(kWood, shade));
		renderer->DrawTriangle(fireX, bottom + 2.0f, 9.0f, 13.0f, Tint(kEmber, shade));
		renderer->DrawTriangle(fireX, bottom + 0.5f, 5.0f, 8.0f, Tint(kFlame, shade));
	}

	// 동행자의 얼굴. 색이 빠질수록 어둠에 잠식된 것이다.
	if (HasCompanion())
	{
		const float alive = 1.0f - CompanionDarkness();
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

	// 상태 창은 맨 위에. Tab을 누르고 있는 동안만.
	if (m_ShowStatus)
	{
		DrawStatus(renderer);
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
	renderer->SetTag(DrawTag::Minimap);
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

	// 내려놓은 화톳불. 장면에서 직접 찾는다 — 따로 목록을 들고 있지 않다.
	m_Scene.ForEach([&](Actor& actor)
	{
		if (actor.Name() == "Bonfire")
		{
			DrawMapMarker(renderer, kMarkFire, mapX(actor.WorldX()), mapY(actor.WorldY()), 0.0f);
		}
	});

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
	renderer->SetTag(DrawTag::Dialogue);
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
	if ((m_Companion != NULL && m_Companion->IsFrozenAgain()))
	{
		DrawPrompt(renderer, "불을 비춰 다시 깨우기",
		           ToRenderX(m_Companion->WorldX()), ToRenderY(m_Companion->WorldY()) + 40.0f);
	}
}

void Game::DrawDialogue(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Dialogue);
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


void Game::RenderHud(Renderer* renderer)
{
	// 떠오르는 글자는 후처리가 끝난 화면 위에 그려야 또렷하다.
	DrawContext context;
	context.renderer = renderer;
	context.game = this;
	context.cameraX = m_CameraX;
	context.cameraY = m_CameraY;
	context.time = m_Time;
	context.windowWidth = m_WindowWidth;
	context.windowHeight = m_WindowHeight;
	m_Scene.Draw(context, SceneGraph::MaskOf(Actor::Layer::Overlay));

	DrawHud(renderer);
}

// ═══════════════════════════════════════════════════════════════
//  전투 그리기
// ═══════════════════════════════════════════════════════════════




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


void Game::DrawCombatHud(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Hud);
	const float halfWidth = m_WindowWidth * 0.5f;
	const float halfHeight = m_WindowHeight * 0.5f;

	// ── 맞았을 때 화면이 붉게 번쩍인다 ──
	const float maxHealth = Progression::MaxHealth(Stats().level);
	const float ratio = std::max(0.0f, Stats().health / maxHealth);
	float redness = Stats().hurtFlash * 0.22f;
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
	if (Stats().levelUpGlow > 0.0f)
	{
		const Color glow = { kSoulColor[0], kSoulColor[1], kSoulColor[2], Stats().levelUpGlow * 0.5f };
		renderer->DrawCircle(badgeX, badgeY, 21.0f + Stats().levelUpGlow * 8.0f, glow, 0.0f, 4.0f);
	}
	renderer->DrawCircle(badgeX, badgeY, 21.0f, rim);
	renderer->DrawCircle(badgeX, badgeY, 18.5f, backing);
	renderer->DrawLabel("Lv", 10.0f, badgeX - 7.0f, badgeY + 16.0f, dim);
	const std::string levelText = std::to_string(Stats().level);
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

	const float shownRatio = std::max(0.0f, Stats().healthShown / maxHealth);
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

	const std::string healthText = std::to_string(static_cast<int>(std::ceil(Stats().health)))
	                             + "/" + std::to_string(static_cast<int>(maxHealth));
	const float healthWidth = MeasureDigits(renderer, healthText, 12.0f);
	const Color outline = { 0.0f, 0.0f, 0.0f, 0.8f };
	DrawDigits(renderer, healthText, 12.0f, trackLeft + barWidth * 0.5f - healthWidth * 0.5f + 1.0f, barY + 7.0f, outline);
	DrawDigits(renderer, healthText, 12.0f, trackLeft + barWidth * 0.5f - healthWidth * 0.5f, barY + 8.0f, text);

	// ── 영혼(경험치) ──
	const float soulY = barY - 15.0f;
	const float needed = static_cast<float>(Progression::SoulsToNextLevel(Stats().level));
	const float soulRatio = std::min(1.0f, Stats().souls / needed);
	renderer->DrawRoundRect(barLeft - 3.0f, soulY, 6.0f, 6.0f, 0.8f, Tint(kSoulColor), 0.7854f);
	renderer->DrawRoundRect(trackLeft + barWidth * 0.5f, soulY, barWidth + 4.0f, 8.0f, 4.0f, backing);
	if (soulRatio > 0.0f)
	{
		renderer->DrawRoundRect(trackLeft + barWidth * soulRatio * 0.5f, soulY, barWidth * soulRatio, 4.0f, 2.0f, Tint(kSoulColor));
	}
	const std::string soulText = std::to_string(Stats().souls) + "/" + std::to_string(static_cast<int>(needed));
	DrawDigits(renderer, soulText, 11.0f, trackLeft + barWidth + 8.0f, soulY + 7.5f, dim);

	// ── 무기 강화와 자석 ──
	const float rowY = soulY - 17.0f;
	renderer->DrawSegment(barLeft - 7.0f, rowY - 4.0f, barLeft + 1.0f, rowY + 4.0f, 2.2f, Tint(kSteel));
	renderer->DrawSegment(barLeft - 7.5f, rowY + 0.5f, barLeft - 2.5f, rowY - 4.5f, 1.8f, Tint(kGold));
	for (int i = 0; i < Progression::kMaxWeaponTier; ++i)
	{
		const bool earned = i < Stats().weaponTier;
		renderer->DrawRoundRect(trackLeft + 4.0f + i * 13.0f, rowY, 9.0f, 5.0f, 2.0f,
		                        earned ? Tint(kGold) : Tint(kStoneDark, 1.2f));
	}

	if (Stats().magnetTime > 0.0f)
	{
		const float magnetX = trackLeft + 86.0f;
		const Color red = Tint(kMagnetRed, 1.2f);
		renderer->DrawRoundRect(magnetX, rowY - 3.0f, 9.0f, 3.2f, 1.6f, red);
		renderer->DrawSegment(magnetX - 3.2f, rowY - 3.0f, magnetX - 3.2f, rowY + 3.5f, 2.6f, red);
		renderer->DrawSegment(magnetX + 3.2f, rowY - 3.0f, magnetX + 3.2f, rowY + 3.5f, 2.6f, red);

		const float remain = Stats().magnetTime / 10.0f;
		renderer->DrawRoundRect(magnetX + 12.0f + 40.0f * remain * 0.5f, rowY, 40.0f * remain, 4.0f, 2.0f, Tint(kSoulColor));
	}

	// ── 종탑지기와 싸우는 중이면 화면 위쪽에 큰 체력 막대 ──
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		const EnemyActor* enemy = m_Enemies[i];
		if (!enemy->IsWarden() || !enemy->IsEngaged() || enemy->IsDying())
		{
			continue;
		}

		const float wardenRatio = std::max(0.0f, enemy->Health() / enemy->MaxHealth());
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

// ═══════════════════════════════════════════════════════════════════
//  한 판의 시작
// ═══════════════════════════════════════════════════════════════════

void Game::Initialize(int windowWidth, int windowHeight, unsigned int seed)
{
	m_WindowWidth = windowWidth;
	m_WindowHeight = windowHeight;

	// 씨앗 하나로 섬 전체가 정해진다. 실행할 때마다 다른 섬이 된다.
	m_World = World(seed);
	m_Regions = Regions(seed);
	m_Random.seed(seed);
	m_Light.Configure(kLightCell);

	// 첫 종탑에서 열네 걸음 떨어진 곳에서 시작한다. 종탑이 선 줄의 십자길 위라
	// 어느 씨앗에서도 걸을 수 있고, 연기 너머로 종탑과 그 앞의 종탑지기가 보인다.
	// 종탑지기가 덤벼드는 거리보다 멀어서 먼저 강해질 여유가 있다.
	const Regions::Region first = m_Regions.At(0, 0);
	m_StartX = TileToWorld(first.towerTileX - 14);
	m_StartY = TileToWorld(first.towerTileY);
	m_PlayerX = m_StartX;
	m_PlayerY = m_StartY;
	m_CameraX = m_StartX;
	m_CameraY = m_StartY;

	// 장면에 땅과 플레이어를 놓는다. 나머지는 걸어 들어가는 대로 놓인다.
	m_Scene.Spawn(std::unique_ptr<Actor>(new GroundActor()));
	m_Player = static_cast<PlayerActor*>(
		m_Scene.Spawn(std::unique_ptr<Actor>(new PlayerActor(m_StartX, m_StartY))));

	RefreshNearbyRegions();
	StreamActors();
}

const PlayerCombat& Game::Stats() const
{
	return m_Player->Combat();
}

int Game::Level() const { return m_Player != NULL ? m_Player->Combat().level : 1; }
float Game::Health() const { return m_Player != NULL ? m_Player->Combat().health : 0.0f; }
float Game::Oil() const { return m_Player != NULL ? m_Player->Oil() : 0.0f; }
float Game::CompanionDarkness() const { return m_Companion != NULL ? m_Companion->Darkness() : 0.0f; }

// ═══════════════════════════════════════════════════════════════════
//  액터 놓고 치우기
//
//  맵이 무한하므로 지형에서 나온 액터를 전부 들고 있을 수는 없다. 가까운
//  청크의 것만 놓고, 멀어지면 치운다. 지형은 좌표로 결정되므로 다시 돌아오면
//  똑같이 되살아난다 — 바뀐 것(종을 울렸는가, 사람을 깨웠는가)만 Regions가 기억한다.
// ═══════════════════════════════════════════════════════════════════

namespace
{
	long long ChunkKey(int chunkX, int chunkY)
	{
		// 구역 키와 겹치지 않도록 청크 키는 위쪽 비트를 비워 둔다.
		return ((static_cast<long long>(chunkY) & 0xFFFFFFFFll) << 32)
		     | (static_cast<long long>(chunkX) & 0xFFFFFFFFll) | (1ll << 62);
	}

	long long RegionKey(int regionX, int regionY)
	{
		return ((static_cast<long long>(regionY) & 0xFFFFFFFFll) << 32)
		     | (static_cast<long long>(regionX) & 0xFFFFFFFFll) | (1ll << 61);
	}
}

void Game::LoadChunk(int chunkX, int chunkY)
{
	const long long key = ChunkKey(chunkX, chunkY);
	const int firstTileX = chunkX * World::kChunkTiles;
	const int firstTileY = chunkY * World::kChunkTiles;
	const int lastTileX = firstTileX + World::kChunkTiles - 1;
	const int lastTileY = firstTileY + World::kChunkTiles - 1;

	std::vector<World::House> houses;
	std::vector<World::Tree> trees;
	m_World.CollectStructures(firstTileX, firstTileY, lastTileX, lastTileY, houses, trees);

	for (size_t i = 0; i < houses.size(); ++i)
	{
		// 집은 여러 칸에 걸치므로 왼쪽 위 칸이 속한 청크에서만 놓는다.
		if (houses[i].tileX < firstTileX || houses[i].tileY < firstTileY)
		{
			continue;
		}
		Actor* actor = m_Scene.Spawn(std::unique_ptr<Actor>(new HouseActor(houses[i])));
		actor->SetStreamKey(key);
	}

	for (size_t i = 0; i < trees.size(); ++i)
	{
		Actor* actor = m_Scene.Spawn(std::unique_ptr<Actor>(new TreeActor(trees[i])));
		actor->SetStreamKey(key);
	}

	std::vector<World::Prop> props;
	m_World.CollectProps(firstTileX, firstTileY, lastTileX, lastTileY, props);
	for (size_t i = 0; i < props.size(); ++i)
	{
		Actor* actor = m_Scene.Spawn(std::unique_ptr<Actor>(new PropActor(props[i])));
		actor->SetStreamKey(key);
	}

	m_LoadedChunks.insert(key);
}

void Game::LoadRegion(int regionX, int regionY)
{
	const long long key = RegionKey(regionX, regionY);
	const Regions::Region region = m_Regions.At(regionX, regionY);

	Actor* tower = m_Scene.Spawn(std::unique_ptr<Actor>(new TowerActor(
		regionX, regionY, TileToWorld(region.towerTileX), TileToWorld(region.towerTileY))));
	tower->SetStreamKey(key);

	// 이미 따라나선 사람 자리에는 아무도 서 있지 않는다.
	if (!region.woken)
	{
		Actor* sleeper = m_Scene.Spawn(std::unique_ptr<Actor>(new SleeperActor(
			regionX, regionY, TileToWorld(region.sleeperTileX), TileToWorld(region.sleeperTileY))));
		sleeper->SetStreamKey(key);
	}

	m_LoadedRegions.insert(key);
}

void Game::UnloadKey(long long key)
{
	// 꼬리표가 같은 액터를 모두 지운다. 포인터를 따로 들고 있지 않으므로
	// 액터가 스스로 사라진 뒤에 다시 지우려다 어긋날 일이 없다.
	m_Scene.ForEach([key](Actor& actor)
	{
		if (actor.StreamKey() == key)
		{
			actor.Destroy();
		}
	});
}

void Game::StreamActors()
{
	// 화면보다 한 청크쯤 넉넉하게 — 화면 가장자리에서 집이 불쑥 나타나지 않게.
	const float margin = static_cast<float>(World::kChunkTiles * kTileSize);
	const int firstChunkX = World::FloorDiv(FloorToInt((m_PlayerX - m_WindowWidth * 0.5f - margin) / kTileSize), World::kChunkTiles);
	const int lastChunkX  = World::FloorDiv(FloorToInt((m_PlayerX + m_WindowWidth * 0.5f + margin) / kTileSize), World::kChunkTiles);
	const int firstChunkY = World::FloorDiv(FloorToInt((m_PlayerY - m_WindowHeight * 0.5f - margin) / kTileSize), World::kChunkTiles);
	const int lastChunkY  = World::FloorDiv(FloorToInt((m_PlayerY + m_WindowHeight * 0.5f + margin) / kTileSize), World::kChunkTiles);

	std::unordered_set<long long> wanted;
	for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY)
	{
		for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX)
		{
			const long long key = ChunkKey(chunkX, chunkY);
			wanted.insert(key);
			if (m_LoadedChunks.find(key) == m_LoadedChunks.end())
			{
				LoadChunk(chunkX, chunkY);
			}
		}
	}

	for (std::unordered_set<long long>::iterator it = m_LoadedChunks.begin(); it != m_LoadedChunks.end(); )
	{
		if (wanted.find(*it) == wanted.end())
		{
			UnloadKey(*it);
			it = m_LoadedChunks.erase(it);
		}
		else
		{
			++it;
		}
	}

	// 종탑은 멀리서도 보여야 하므로 구역은 더 넉넉히 들고 있는다.
	std::unordered_set<long long> wantedRegions;
	for (size_t i = 0; i < m_NearbyRegions.size(); ++i)
	{
		const long long key = RegionKey(m_NearbyRegions[i].regionX, m_NearbyRegions[i].regionY);
		wantedRegions.insert(key);
		if (m_LoadedRegions.find(key) == m_LoadedRegions.end())
		{
			LoadRegion(m_NearbyRegions[i].regionX, m_NearbyRegions[i].regionY);
		}
	}

	for (std::unordered_set<long long>::iterator it = m_LoadedRegions.begin(); it != m_LoadedRegions.end(); )
	{
		if (wantedRegions.find(*it) == wantedRegions.end())
		{
			UnloadKey(*it);
			it = m_LoadedRegions.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// ═══════════════════════════════════════════════════════════════════
//  세계
// ═══════════════════════════════════════════════════════════════════

void Game::BuildLights()
{
	// 빛은 액터들에게서 모은다. 등불도 화톳불도 스스로 빛난다고 말할 뿐이고,
	// 그것을 격자에 옮겨 담는 일은 여기서 한 번에 한다.
	m_Light.BeginFrame();
	m_Scene.ForEach([this](Actor& actor)
	{
		float radius = 0.0f;
		bool leavesTrail = false;
		if (actor.IsActive() && actor.GetLight(radius, leavesTrail))
		{
			m_Light.AddLight(actor.WorldX(), actor.WorldY(), radius, leavesTrail);
		}
	});
}

bool Game::IsBodyBlocked(float worldX, float worldY, float halfSize) const
{
	// 네 모서리를 본다. 사각형끼리의 판정이라 렌더러가 그리는 것과 같은 수학이다.
	return IsBlockedAtWorld(worldX - halfSize, worldY - halfSize)
	    || IsBlockedAtWorld(worldX + halfSize, worldY - halfSize)
	    || IsBlockedAtWorld(worldX - halfSize, worldY + halfSize)
	    || IsBlockedAtWorld(worldX + halfSize, worldY + halfSize);
}

bool Game::MoveBody(float& x, float& y, float dx, float dy, float radius, bool avoidBeacons) const
{
	auto blocked = [&](float px, float py)
	{
		if (IsBodyBlocked(px, py, radius * 0.7f))
		{
			return true;
		}
		// 종소리가 닿는 곳에는 어둠이 들어오지 못한다.
		return avoidBeacons && BeaconBrightness(px, py) > 0.0f;
	};

	bool moved = false;
	if (dx != 0.0f && !blocked(x + dx, y)) { x += dx; moved = true; }
	if (dy != 0.0f && !blocked(x, y + dy)) { y += dy; moved = true; }
	return moved;
}

float Game::GlowAt(float worldX, float worldY) const
{
	return m_Light.Glow(m_Light.CellOf(worldX), m_Light.CellOf(worldY));
}

float Game::PowerAt(float worldX, float worldY) const
{
	// 처음 마을에서 멀어질수록 적이 강해진다. 마을 하나를 건널 때마다 18%.
	const int regionX = Regions::RegionOfTile(FloorToInt(worldX / kTileSize));
	const int regionY = Regions::RegionOfTile(FloorToInt(worldY / kTileSize));
	const int villagesAway = std::max(std::abs(regionX), std::abs(regionY));
	return 1.0f + 0.18f * static_cast<float>(villagesAway);
}

bool Game::IsRingingAt(int regionX, int regionY) const
{
	return m_RingProgress > 0.0f && m_RingAvailable
	    && Regions::RegionOfTile(FloorToInt(m_RingTowerX / kTileSize)) == regionX
	    && Regions::RegionOfTile(FloorToInt(m_RingTowerY / kTileSize)) == regionY;
}

bool Game::IsTalkingWith(int regionX, int regionY) const
{
	return m_Dialogue.active && m_Dialogue.regionX == regionX && m_Dialogue.regionY == regionY;
}

float Game::RandomRange(float low, float high)
{
	std::uniform_real_distribution<float> distribution(low, high);
	return distribution(m_Random);
}

bool Game::Roll(int percent)
{
	return RandomRange(0.0f, 100.0f) < static_cast<float>(percent);
}

void Game::RegisterEnemy(EnemyActor* enemy)
{
	m_Enemies.push_back(enemy);
}

void Game::UnregisterEnemy(EnemyActor* enemy)
{
	m_Enemies.erase(std::remove(m_Enemies.begin(), m_Enemies.end(), enemy), m_Enemies.end());
}

EnemyActor* Game::FindEnemy(int id) const
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		if (m_Enemies[i]->Id() == id)
		{
			return m_Enemies[i];
		}
	}
	return NULL;
}

void Game::SpawnFloatingText(float worldX, float worldY, const std::string& label, int number,
                             const float color[3], float size)
{
	m_Scene.Spawn(std::unique_ptr<Actor>(new FloatingTextActor(
		worldX + RandomRange(-6.0f, 6.0f), worldY, label, number, color, size)));
}

void Game::NotifyWardenDefeated(int regionX, int regionY)
{
	m_Regions.MarkWardenDefeated(regionX, regionY);

	// 흩뿌려 놓은 구슬도 함께 흩어진다.
	m_Scene.ForEach([](Actor& actor)
	{
		if (actor.Name() == "Orb")
		{
			actor.Destroy();
		}
	});

	RefreshNearbyRegions();
}

void Game::RequestRespawn()
{
	if (m_PendingRespawn)
	{
		return;
	}
	m_PendingRespawn = true;
	m_RespawnTimer = 1.4f;
}

// ═══════════════════════════════════════════════════════════════════
//  적이 나타나고, 쓰러지고, 다시 일어서는 일
// ═══════════════════════════════════════════════════════════════════

void Game::SpawnWardens()
{
	for (size_t i = 0; i < m_NearbyRegions.size(); ++i)
	{
		const Regions::Region& region = m_NearbyRegions[i];
		if (region.rung || region.wardenDefeated)
		{
			continue;
		}

		const float towerX = TileToWorld(region.towerTileX);
		const float towerY = TileToWorld(region.towerTileY);
		if (Distance(m_PlayerX, m_PlayerY, towerX, towerY) > 900.0f)
		{
			continue;
		}

		bool present = false;
		for (size_t k = 0; k < m_Enemies.size(); ++k)
		{
			if (m_Enemies[k]->IsWarden() && !m_Enemies[k]->IsDying()
			 && m_Enemies[k]->Data().regionX == region.regionX
			 && m_Enemies[k]->Data().regionY == region.regionY)
			{
				present = true;
				break;
			}
		}
		if (present)
		{
			continue;
		}

		// 종탑 앞 광장에 선다. 멀리서도 보이도록 — 준비될 때까지 다가가지 말라는 경고다.
		std::unique_ptr<EnemyActor> warden(new EnemyActor(EnemyKind::Warden, towerX, towerY + 72.0f,
		                                                  PowerAt(towerX, towerY)));
		warden->MakeWarden(region.regionX, region.regionY, towerX, towerY + 72.0f);
		warden->AttachToGame(this);
		m_Scene.Spawn(std::unique_ptr<Actor>(warden.release()));
	}
}

void Game::SpawnEnemies(float deltaSeconds)
{
	m_SpawnTimer -= deltaSeconds;
	if (m_SpawnTimer > 0.0f || m_PendingRespawn)
	{
		return;
	}
	m_SpawnTimer = 2.4f * RandomRange(0.7f, 1.3f);

	// 되찾은 구역 안에 서 있으면 어둠이 다가오지 않는다.
	if (BeaconBrightness(m_PlayerX, m_PlayerY) > 0.0f)
	{
		return;
	}

	int alive = 0;
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		if (!m_Enemies[i]->IsWarden() && !m_Enemies[i]->IsDying())
		{
			++alive;
		}
	}

	const float power = PowerAt(m_PlayerX, m_PlayerY);
	const int villagesAway = static_cast<int>((power - 1.0f) / 0.18f + 0.5f);
	const int limit = std::min(16, 5 + Stats().level / 2 + villagesAway);
	if (alive >= limit)
	{
		return;
	}

	// 처음 마을에서 레벨이 낮을 때는 약한 그림자만 나온다. 파밍을 익히는 곳이다.
	const bool practice = (villagesAway == 0 && Stats().level < 3);
	const int huskPercent = practice ? 0 : std::min(60, 22 + villagesAway * 6 + Stats().level);

	const int group = std::min(limit - alive, 1 + static_cast<int>(RandomRange(0.0f, 2.99f)));
	for (int g = 0; g < group; ++g)
	{
		for (int attempt = 0; attempt < 10; ++attempt)
		{
			const float angle = RandomRange(0.0f, 6.2831853f);
			const float distance = RandomRange(430.0f, 640.0f);
			const float x = m_PlayerX + std::cos(angle) * distance;
			const float y = m_PlayerY + std::sin(angle) * distance;

			// 걸을 수 있는 칸에만. 맵 생성이 모든 걸을 수 있는 칸을 길과 이어 두었으므로
			// 여기서 나타난 적은 반드시 플레이어에게 닿을 수 있다.
			if (IsBlockedAtWorld(x, y) || BeaconBrightness(x, y) > 0.0f
			 || TotalBrightness(x, y) >= 0.2f)
			{
				continue;
			}

			std::unique_ptr<EnemyActor> enemy(new EnemyActor(
				Roll(huskPercent) ? EnemyKind::Husk : EnemyKind::Wraith, x, y, PowerAt(x, y)));
			enemy->AttachToGame(this);
			m_Scene.Spawn(std::unique_ptr<Actor>(enemy.release()));
			break;
		}
	}
}

void Game::UpdateSpawning(float deltaSeconds)
{
	SpawnWardens();
	SpawnEnemies(deltaSeconds);

	if (m_PendingRespawn)
	{
		m_RespawnTimer -= deltaSeconds;
		if (m_RespawnTimer <= 0.0f)
		{
			RespawnPlayer();
		}
	}
}

void Game::RespawnPlayer()
{
	// 쓰러지면 가장 가까운 되찾은 종탑 앞, 없으면 처음 자리에서 다시 일어선다.
	float bestX = m_StartX;
	float bestY = m_StartY;
	float best = Distance(m_PlayerX, m_PlayerY, m_StartX, m_StartY);

	std::vector<Regions::Region> rung;
	m_Regions.CollectRung(rung);
	for (size_t i = 0; i < rung.size(); ++i)
	{
		const float x = TileToWorld(rung[i].towerTileX);
		const float y = TileToWorld(rung[i].towerTileY) + 64.0f;
		const float distance = Distance(m_PlayerX, m_PlayerY, x, y);
		if (distance < best)
		{
			best = distance;
			bestX = x;
			bestY = y;
		}
	}

	m_Player->PlaceAt(bestX, bestY);
	m_PlayerX = bestX;
	m_PlayerY = bestY;
	m_CameraX = bestX;
	m_CameraY = bestY;

	// 어둠은 흩어지고, 곁에 있던 사람은 함께 옮겨 온다.
	m_Scene.ForEach([](Actor& actor)
	{
		if (actor.GetLayer() == Actor::Layer::Effect || actor.Name() == "Enemy")
		{
			actor.Destroy();
		}
	});
	m_Enemies.clear();

	if (m_Companion != NULL)
	{
		m_Companion->SetPosition(bestX - 24.0f, bestY);
	}

	PlayerCombat& stats = m_Player->Combat();
	stats.health = m_Player->MaxHealth();
	stats.healthShown = stats.health;
	stats.invulnerable = 2.0f;
	stats.hurtFlash = 0.0f;
	++stats.deaths;
	m_Player->SetOil(std::max(m_Player->Oil(), 0.5f));

	m_SpawnTimer = 4.0f;
	m_PendingRespawn = false;

	RefreshNearbyRegions();
	const float gold[3] = { 1.0f, 0.82f, 0.4f };
	SpawnFloatingText(bestX, bestY - 60.0f, "다시 일어섰다", -1, gold, 18.0f);
}

// ═══════════════════════════════════════════════════════════════════
//  한 프레임
// ═══════════════════════════════════════════════════════════════════

void Game::Update(float deltaSeconds, const Input& input)
{
	m_ShowStatus = input.HoldingStatus();

	m_Time += deltaSeconds;
	m_MapAge += deltaSeconds;

	m_PlayerX = m_Player->WorldX();
	m_PlayerY = m_Player->WorldY();

	// 대화 중에는 세계가 멈춘다. 기름도 줄지 않고, 발자국도 식지 않고,
	// 적도 다가오지 않는다. 불꽃과 까마귀는 그리기 쪽 시계로 계속 움직인다.
	if (m_Dialogue.active)
	{
		UpdateDialogue(deltaSeconds, input);
		BuildLights();
		return;
	}

	RefreshNearbyRegions();
	StreamActors();

	// 조명을 먼저 세운다. 액터들이 "여기 밝은가"를 물을 때 이번 프레임의 등불을 보도록.
	BuildLights();
	RecordExplored();

	ActorContext context;
	context.deltaSeconds = deltaSeconds;
	context.time = m_Time;
	context.game = this;
	context.scene = &m_Scene;
	context.input = &input;
	context.player = m_Player;
	m_Scene.Update(context);

	// 액터들이 움직인 뒤의 자리
	m_PlayerX = m_Player->WorldX();
	m_PlayerY = m_Player->WorldY();

	// 카메라는 살짝 늦게 따라온다. 방향을 꺾을 때 화면이 덜 튄다.
	const float follow = std::min(1.0f, deltaSeconds * 8.0f);
	m_CameraX += (m_PlayerX - m_CameraX) * follow;
	m_CameraY += (m_PlayerY - m_CameraY) * follow;

	HandleInteraction(deltaSeconds, input);
	UpdateSpawning(deltaSeconds);

	m_Light.Decay(deltaSeconds);
}

void Game::Render(Renderer* renderer)
{
	DrawContext context;
	context.renderer = renderer;
	context.game = this;
	context.cameraX = m_CameraX;
	context.cameraY = m_CameraY;
	context.time = m_Time;
	context.windowWidth = m_WindowWidth;
	context.windowHeight = m_WindowHeight;

	// 땅 → 서 있는 것들 → 연무 → 날아다니는 것들. 같은 층 안에서는 발밑이 아래인 것이 앞이다.
	// 연무를 날아다니는 것들보다 먼저 까는 것은, 불씨와 스킬 효과가 안개를
	// 뚫고 보여야 싸움이 읽히기 때문이다.
	m_Scene.Draw(context, SceneGraph::MaskOf(Actor::Layer::Ground)
	                    | SceneGraph::MaskOf(Actor::Layer::Object));
	DrawMist(renderer);
	m_Scene.Draw(context, SceneGraph::MaskOf(Actor::Layer::Effect));
}

//
// 어둠 위에 깔리는 연무.
//
// 화면을 40px 칸으로 나눠 칸마다 안개가 얼마나 짙은지 정하고, 그걸 작은
// 그림 한 장(27x19)에 담아 화면 크기로 부드럽게 늘려 그린다. 늘릴 때
// 이웃 칸끼리 섞이므로 칸의 경계는 보이지 않고 은은한 연무만 남는다.
//
// 큰 원 수십 개를 겹쳐 그리는 방법(독초 연기처럼)도 있지만, 안개는 크고
// 가장자리가 넓어 화면을 스무 번 넘게 덧칠하게 된다. 이 방법은 한 번이다.
//
// 짙기는 두 가지로 정한다.
//   · 밝기 — 종소리가 닿는 곳, 등불이 비추는 곳, 발자국이 남은 곳에서 걷힌다
//   · 노이즈 — 시간에 따라 천천히 흐르는 결. 같은 어둠도 짙고 옅은 데가 있다
//
// 칸은 세계에 박혀 있다(카메라가 아니라). 그래서 걸어가도 안개가 따라오지
// 않고 제자리에서 흐른다.
//
// 안개는 발광을 가리지 않는다. 짙은 어둠 너머에서도 적의 붉은 눈은 번진다.
//
namespace
{
	float Hash01(int x, int y)
	{
		return static_cast<float>(PropHash(x * 7 + 101, y * 13 + 57) & 0xffffu) / 65535.0f;
	}

	// 격자 꼭짓점의 값을 부드럽게 이은 노이즈. 0~1.
	float SmoothNoise(float x, float y)
	{
		const int ix = FloorToInt(x);
		const int iy = FloorToInt(y);
		float fx = x - static_cast<float>(ix);
		float fy = y - static_cast<float>(iy);
		fx = fx * fx * (3.0f - 2.0f * fx);
		fy = fy * fy * (3.0f - 2.0f * fy);
		const float a = Hash01(ix, iy);
		const float b = Hash01(ix + 1, iy);
		const float c = Hash01(ix, iy + 1);
		const float d = Hash01(ix + 1, iy + 1);
		return Mix(Mix(a, b, fx), Mix(c, d, fx), fy);
	}

	float SmoothStep(float edge0, float edge1, float x)
	{
		const float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
		return t * t * (3.0f - 2.0f * t);
	}

	const float kMistCell = 40.0f;          // 칸 하나의 크기(px)
	const float kMistAlpha = 0.62f;         // 가장 짙을 때
	const float kMistClearFrom = 0.24f;     // 이 밝기부터 걷히기 시작해서
	const float kMistClearTo = 0.44f;       // 이 밝기면 다 걷힌다(발자국 밝기 0.42도 걷힌다)
}

void Game::DrawMist(Renderer* renderer)
{
	if (!m_ShowMist)
	{
		return;
	}
	renderer->SetTag(DrawTag::Mist);

	if (m_MistImage == 0)
	{
		m_MistColumns = static_cast<int>(m_WindowWidth / kMistCell) + 3;
		m_MistRows = static_cast<int>(m_WindowHeight / kMistCell) + 3;
		m_MistImage = renderer->CreateImage(m_MistColumns, m_MistRows, true);
		m_MistPixels.assign(static_cast<size_t>(m_MistColumns * m_MistRows * 4), 0);
	}

	const int originX = FloorToInt((m_CameraX - m_WindowWidth * 0.5f) / kMistCell) - 1;
	const int originY = FloorToInt((m_CameraY - m_WindowHeight * 0.5f) / kMistCell) - 1;

	// 짙은 어둠의 색. 배경(연기)보다 조금 더 차고 어둡다.
	const unsigned char red = 2;
	const unsigned char green = 3;
	const unsigned char blue = 6;

	for (int row = 0; row < m_MistRows; ++row)
	{
		for (int column = 0; column < m_MistColumns; ++column)
		{
			const int cellX = originX + column;
			const int cellY = originY + row;
			const float worldX = (static_cast<float>(cellX) + 0.5f) * kMistCell;
			const float worldY = (static_cast<float>(cellY) + 0.5f) * kMistCell;

			const float clear = SmoothStep(kMistClearFrom, kMistClearTo, TotalBrightness(worldX, worldY));

			// 크고 느린 결 위에 작고 조금 빠른 결을 얹는다
			const float slow = SmoothNoise(cellX * 0.18f + m_Time * 0.05f, cellY * 0.18f + m_Time * 0.03f);
			const float quick = SmoothNoise(cellX * 0.45f - m_Time * 0.09f, cellY * 0.45f + m_Time * 0.07f);
			const float grain = 0.62f * slow + 0.38f * quick;

			const float density = (0.5f + 0.5f * grain) * (1.0f - clear);

			unsigned char* pixel = &m_MistPixels[static_cast<size_t>((row * m_MistColumns + column) * 4)];
			pixel[0] = red;
			pixel[1] = green;
			pixel[2] = blue;
			pixel[3] = ToByte(density * kMistAlpha);
		}
	}

	renderer->UpdateImage(m_MistImage, m_MistColumns, m_MistRows, m_MistPixels.data());
	renderer->DrawImage(m_MistImage,
	                    ToRenderX(originX * kMistCell), ToRenderY(originY * kMistCell),
	                    m_MistColumns * kMistCell, m_MistRows * kMistCell,
	                    1.0f, false);
	renderer->SetTag(DrawTag::Other);
}

void Game::DrawFloatingText(const DrawContext& context, const FloatingText& text,
                            float worldX, float worldY) const
{
	const float t = text.age / text.life;
	const float alpha = std::max(0.0f, 1.0f - t * t);
	const float x = context.ToRenderX(worldX);
	const float y = context.ToRenderY(worldY);
	const float top = y + text.size * 0.6f;

	const Color color = { text.r, text.g, text.b, alpha };
	const Color shadow = { 0.0f, 0.0f, 0.0f, alpha * 0.75f };

	Game* self = const_cast<Game*>(this);   // 글자 텍스처 캐시를 쓰기 위한 것뿐이다
	Renderer* renderer = context.renderer;

	if (text.number >= 0)
	{
		const std::string digits = std::to_string(text.number);
		const float width = self->MeasureDigits(renderer, digits, text.size);
		self->DrawDigits(renderer, digits, text.size, x - width * 0.5f + 1.0f, top - 1.0f, shadow);
		self->DrawDigits(renderer, digits, text.size, x - width * 0.5f, top, color);
	}
	else
	{
		const float width = renderer->MeasureLabel(text.label, text.size);
		renderer->DrawLabel(text.label, text.size, x - width * 0.5f + 1.0f, top - 1.0f, shadow);
		renderer->DrawLabel(text.label, text.size, x - width * 0.5f, top, color);
	}
}
