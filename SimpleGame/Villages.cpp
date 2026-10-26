#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Professions.h"
#include "Regions.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace Art;

//
// 마을들의 삶.
//
// 사람이 사는 마을은 첫 마을과 종을 울린 마을이다. 종이 울려 빛이 돌아오면
// 숨어 있던 사람들이 돌아와, 마을마다 스무 명이 저마다의 집에 산다.
//
// 마을은 이웃 마을과 편지로 소식을 주고받는다. 전령이 편지를 들고 숲길을
// 건너고, 그 소식을 들은 사람들은 제 성향에 따라 마음이 움직인다 —
// 욕심 많은 사람은 넉넉한 마을로, 겁 많은 사람은 안전한 마을로, 방랑기 있는
// 사람은 그냥 떠나고 싶어서.
//
// 이사를 결심한 사람은 혼자 숲에 들어가지 않는다. 기사와 모험가에게 품삯을
// 주고 호위를 부탁하고, 그 숲길을 건너기에 넉넉한 힘이 모여야 떠난다
// (RequiredEscort). 일행은 문 안쪽에 모였다가 함께 숲길을 건너고, 닿으면
// 이사 온 사람은 맡아 둔 새 집으로, 호위는 제 마을로 돌아간다.
//
// 숲길에서는 어둠이 사람을 노린다. 플레이어 가까이에서는 진짜 적이 덤비고,
// 멀리 화면 밖에서는 습격을 굴려 호위의 힘과 견준다.
//
namespace
{
	long long HouseKeyOf(int tileX, int tileY)
	{
		return (static_cast<long long>(tileY) << 32) | static_cast<long long>(static_cast<unsigned int>(tileX));
	}

	// 집 앞(문이 난 칸 바로 아래). 집은 앞면이 화면 아래를 향한다.
	void DoorTile(const World::House& house, int& tileX, int& tileY)
	{
		const int offset = (house.door < 0) ? 0 : (house.door == 0 ? house.width / 2 : house.width - 1);
		tileX = house.tileX + offset;
		tileY = house.tileY + house.depth;
	}

	// 마을 하나의 주민. 가게 넷, 기사 둘, 모험가 다섯(갈래마다 하나), 사냥꾼 둘, 경비 둘, 주민 다섯.
	// 모험가가 갈래마다 하나씩 있어서, 어느 마을에서든 파티를 다섯까지 꾸릴 수 있다.
	struct Resident
	{
		Profession profession;
		bool thief;
		Calling calling;
	};
	const Resident kRoster[] =
	{
		{ Profession::Merchant,   false, Calling::None },
		{ Profession::Doctor,     false, Calling::None },
		{ Profession::Informant,  false, Calling::None },
		{ Profession::Courier,    false, Calling::None },
		{ Profession::Knight,     false, Calling::None },
		{ Profession::Knight,     false, Calling::None },
		{ Profession::Adventurer, false, Calling::Warrior },
		{ Profession::Adventurer, false, Calling::Archer },
		{ Profession::Adventurer, false, Calling::Healer },
		{ Profession::Adventurer, false, Calling::Mage },
		{ Profession::Adventurer, false, Calling::Guardian },
		{ Profession::Hunter,     false, Calling::None },
		{ Profession::Hunter,     false, Calling::None },
		{ Profession::Guard,      false, Calling::None },
		{ Profession::Guard,      false, Calling::None },
		{ Profession::Commoner,   true,  Calling::None },   // 좀도둑
		{ Profession::Commoner,   true,  Calling::None },
		{ Profession::Commoner,   false, Calling::None },
		{ Profession::Commoner,   false, Calling::None },
		{ Profession::Commoner,   false, Calling::None },
	};
}

// ═══ 마을 · 집 ════════════════════════════════════════════════════

bool Game::IsPopulated(int village) const
{
	auto found = m_Lives.find(village);
	return found != m_Lives.end() && found->second.populated;
}

int Game::VillageOf(float worldX, float worldY) const
{
	return m_Regions.VillageIndex(Regions::RegionOfTile(FloorToInt(worldX / kTileSize)),
	                              Regions::RegionOfTile(FloorToInt(worldY / kTileSize)));
}

void Game::VillageCenter(int village, float& worldX, float& worldY) const
{
	int regionX = 0;
	int regionY = 0;
	m_Regions.VillagePosition(village, regionX, regionY);
	const Regions::Region region = m_Regions.At(regionX, regionY);
	worldX = TileToWorld(region.towerTileX);
	worldY = TileToWorld(region.towerTileY);
}

void Game::NoteTheft(int village)
{
	auto found = m_Lives.find(village);
	if (found != m_Lives.end())
	{
		found->second.thefts += 1.0f;
	}
}

int Game::NearestPopulatedVillage() const
{
	const int village = VillageOf(m_PlayerX, m_PlayerY);
	return IsPopulated(village) ? village : -1;
}

// 종이 울려 빛이 돌아온 마을에 사람들이 돌아온다. 마을마다 한 번만.
void Game::PopulateVillage(int village)
{
	if (village < 0)
	{
		return;
	}
	VillageLife& life = m_Lives[village];
	if (life.populated)
	{
		return;
	}
	life.populated = true;
	life.mailTimer = RandomRange(40.0f, 70.0f);

	const int count = static_cast<int>(sizeof(kRoster) / sizeof(kRoster[0]));
	for (int i = 0; i < count && i < kVillagersPerVillage; ++i)
	{
		VillagerActor* villager = SpawnVillager(village, kRoster[i].profession, -1,
		                                        static_cast<unsigned int>(m_Random()), kRoster[i].calling);
		if (kRoster[i].thief)
		{
			villager->MakeThief();
		}
	}
}

VillagerActor* Game::SpawnVillager(int village, Profession profession, int perkIndex, unsigned int look,
                                   Calling calling)
{
	float centerX = 0.0f;
	float centerY = 0.0f;
	VillageCenter(village, centerX, centerY);

	float doorX = 0.0f;
	float doorY = 0.0f;
	long long key = -1;
	if (!ClaimHouse(village, profession, doorX, doorY, key))
	{
		// 빈집이 없다. 광장 가에 머문다.
		doorX = centerX + RandomRange(-220.0f, 220.0f);
		doorY = centerY + RandomRange(140.0f, 260.0f);
		for (int tries = 0; tries < 12 && IsBodyBlocked(doorX, doorY, 9.0f); ++tries)
		{
			doorX += RandomRange(-40.0f, 40.0f);
			doorY += RandomRange(-40.0f, 40.0f);
		}
	}

	VillagerActor* villager = static_cast<VillagerActor*>(m_Scene.Spawn(std::unique_ptr<Actor>(
		new VillagerActor(doorX, doorY, look, profession, perkIndex, calling))));
	villager->AttachToGame(this, village, centerX, centerY);
	villager->SetHouse(key);
	return villager;
}

//
// 빈집을 하나 맡는다. 무너지지 않은 집 중에서, 가게 주인은 광장에 가까운
// 집을, 나머지는 아무 집이나. 집 앞(문 아래 칸)이 그 사람의 집 자리가 된다.
//
bool Game::ClaimHouse(int village, Profession profession, float& doorX, float& doorY, long long& key)
{
	int regionX = 0;
	int regionY = 0;
	m_Regions.VillagePosition(village, regionX, regionY);
	const int span = Regions::RegionTileSpan();
	const int minX = regionX * span + World::kWallLow + 1;
	const int maxX = regionX * span + World::kWallHigh - 1;
	const int minY = regionY * span + World::kWallLow + 1;
	const int maxY = regionY * span + World::kWallHigh - 1;

	std::vector<World::House> houses;
	std::vector<World::Tree> trees;
	m_World.CollectStructures(minX, minY, maxX, maxY, houses, trees);

	float centerX = 0.0f;
	float centerY = 0.0f;
	VillageCenter(village, centerX, centerY);
	const bool shop = ProfessionOf(profession).shop;

	int best = -1;
	float bestScore = 1.0e30f;
	for (size_t i = 0; i < houses.size(); ++i)
	{
		const World::House& house = houses[i];
		if (house.ruined || m_Houses.count(HouseKeyOf(house.tileX, house.tileY)) != 0)
		{
			continue;
		}
		int tileX = 0;
		int tileY = 0;
		DoorTile(house, tileX, tileY);
		if (m_World.IsBlocked(tileX, tileY))
		{
			continue;
		}
		const float score = shop ? Distance(TileToWorld(tileX), TileToWorld(tileY), centerX, centerY)
		                         : RandomRange(0.0f, 1.0f);
		if (score < bestScore)
		{
			bestScore = score;
			best = static_cast<int>(i);
		}
	}
	if (best < 0)
	{
		return false;
	}

	const World::House& house = houses[static_cast<size_t>(best)];
	key = HouseKeyOf(house.tileX, house.tileY);
	m_Houses[key] = house;

	int tileX = 0;
	int tileY = 0;
	DoorTile(house, tileX, tileY);
	doorX = TileToWorld(tileX);
	doorY = TileToWorld(tileY);
	return true;
}

void Game::ReleaseHouse(long long key)
{
	m_Houses.erase(key);
}

void Game::DoorOf(long long key, float& doorX, float& doorY) const
{
	auto found = m_Houses.find(key);
	if (found == m_Houses.end())
	{
		return;
	}
	int tileX = 0;
	int tileY = 0;
	DoorTile(found->second, tileX, tileY);
	doorX = TileToWorld(tileX);
	doorY = TileToWorld(tileY);
}

// ═══ 길 찾기 ══════════════════════════════════════════════════════
//
// 칸 단위 너비 우선 탐색. 출발점과 도착점을 감싸는 사각형 안에서만 찾는다.
// 숲길은 굽어 있어도 폭이 좁고, 마을 안은 골목이 엮여 있어 사각형에 넉넉한
// 여백을 두면 늘 길이 나온다. 꺾이는 칸만 남겨 들고 다닌다.
//
// 이사나 편지 길을 떠날 때 한 번씩만 부르므로, 수만 칸을 훑어도 괜찮다.
//
bool Game::FindPath(float fromX, float fromY, float toX, float toY,
                    std::vector<std::pair<float, float> >& out) const
{
	out.clear();

	auto walkable = [&](int tileX, int tileY) { return !m_World.IsBlocked(tileX, tileY); };

	// 몸이 모서리에 걸쳐 있으면 서 있는 칸이 막힌 칸일 수 있다. 가까운 빈 칸에서 출발한다.
	auto settle = [&](int& tileX, int& tileY)
	{
		if (walkable(tileX, tileY))
		{
			return true;
		}
		for (int radius = 1; radius <= 2; ++radius)
		{
			for (int dy = -radius; dy <= radius; ++dy)
			{
				for (int dx = -radius; dx <= radius; ++dx)
				{
					if (walkable(tileX + dx, tileY + dy))
					{
						tileX += dx;
						tileY += dy;
						return true;
					}
				}
			}
		}
		return false;
	};

	int startX = FloorToInt(fromX / kTileSize);
	int startY = FloorToInt(fromY / kTileSize);
	int goalX = FloorToInt(toX / kTileSize);
	int goalY = FloorToInt(toY / kTileSize);
	if (!settle(startX, startY) || !settle(goalX, goalY))
	{
		return false;
	}

	const int margin = 24;
	const int minX = std::min(startX, goalX) - margin;
	const int minY = std::min(startY, goalY) - margin;
	const int width = std::max(startX, goalX) + margin - minX + 1;
	const int height = std::max(startY, goalY) + margin - minY + 1;
	if (static_cast<long long>(width) * height > 600000)
	{
		return false;
	}

	std::vector<int> cameFrom(static_cast<size_t>(width) * height, -2);   // -2 아직, -1 출발
	std::vector<int> queue;
	queue.reserve(8192);

	const int start = (startY - minY) * width + (startX - minX);
	const int goal = (goalY - minY) * width + (goalX - minX);
	cameFrom[static_cast<size_t>(start)] = -1;
	queue.push_back(start);

	const int stepX[4] = { 1, -1, 0, 0 };
	const int stepY[4] = { 0, 0, 1, -1 };
	bool found = false;
	for (size_t head = 0; head < queue.size(); ++head)
	{
		const int cell = queue[head];
		if (cell == goal)
		{
			found = true;
			break;
		}
		const int cellX = cell % width;
		const int cellY = cell / width;
		for (int k = 0; k < 4; ++k)
		{
			const int nx = cellX + stepX[k];
			const int ny = cellY + stepY[k];
			if (nx < 0 || ny < 0 || nx >= width || ny >= height)
			{
				continue;
			}
			const int next = ny * width + nx;
			if (cameFrom[static_cast<size_t>(next)] != -2 || !walkable(nx + minX, ny + minY))
			{
				continue;
			}
			cameFrom[static_cast<size_t>(next)] = cell;
			queue.push_back(next);
		}
	}
	if (!found)
	{
		return false;
	}

	std::vector<int> cells;
	for (int cell = goal; cell != -1; cell = cameFrom[static_cast<size_t>(cell)])
	{
		cells.push_back(cell);
	}
	std::reverse(cells.begin(), cells.end());

	// 꺾이는 칸만 남긴다. 그 사이는 곧은 길이라 그대로 걸어가면 된다.
	for (size_t i = 0; i < cells.size(); ++i)
	{
		const bool keep = (i == 0 || i + 1 == cells.size());
		bool turn = false;
		if (!keep)
		{
			const int ax = cells[i - 1] % width - cells[i] % width;
			const int ay = cells[i - 1] / width - cells[i] / width;
			const int bx = cells[i] % width - cells[i + 1] % width;
			const int by = cells[i] / width - cells[i + 1] / width;
			turn = (ax != bx || ay != by);
		}
		if (keep || turn)
		{
			out.push_back(std::make_pair(TileToWorld(cells[i] % width + minX), TileToWorld(cells[i] / width + minY)));
		}
	}
	return true;
}

// 문 바로 안쪽, 안길과 큰길이 만나는 자리. 이웃 마을로 떠나는 일행이 모이는 곳이다.
bool Game::GatePoint(int village, int toward, float& worldX, float& worldY) const
{
	int ax = 0, ay = 0, bx = 0, by = 0;
	m_Regions.VillagePosition(village, ax, ay);
	m_Regions.VillagePosition(toward, bx, by);
	const int dx = (bx > ax) - (bx < ax);
	const int dy = (by > ay) - (by < ay);
	if (dx == 0 && dy == 0)
	{
		return false;
	}

	float centerX = 0.0f;
	float centerY = 0.0f;
	VillageCenter(village, centerX, centerY);

	// 큰길(두 칸)의 한가운데는 종탑 칸보다 반 칸 위 · 왼쪽이다
	const float along = VillageLane();
	const float half = 0.5f * static_cast<float>(kTileSize);
	worldX = centerX + static_cast<float>(dx) * along - (dx == 0 ? half : 0.0f);
	worldY = centerY + static_cast<float>(dy) * along - (dy == 0 ? half : 0.0f);
	return true;
}

void Game::SendHome(VillagerActor* villager, float fromX, float fromY)
{
	std::vector<std::pair<float, float> > path;
	if (!FindPath(fromX, fromY, villager->HomeX(), villager->HomeY(), path))
	{
		path.clear();
		path.push_back(std::make_pair(villager->HomeX(), villager->HomeY()));
	}
	villager->SetPath(path);
}

// ═══ 소식 ═════════════════════════════════════════════════════════

void Game::UpdateVillages(float deltaSeconds)
{
	UpdateTrips(deltaSeconds);

	m_VillageTick -= deltaSeconds;
	if (m_VillageTick > 0.0f)
	{
		return;
	}
	m_VillageTick = 1.0f;

	for (auto& entry : m_Lives)
	{
		if (entry.second.populated)
		{
			UpdateVillageLife(entry.first, entry.second);
		}
	}
	FormTrips();
}

// 1초마다. 이 마을이 얼마나 넉넉하고 얼마나 위험한지 다시 잰다. 편지도 쓴다.
void Game::UpdateVillageLife(int village, VillageLife& life)
{
	int residents = 0;
	float gold = 0.0f;
	float decor = 0.0f;
	float hurt = 0.0f;
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		const VillagerActor* villager = m_Villagers[i];
		if (villager->Village() != village || villager->IsDead() || villager->Trip() >= 0 || villager->IsHired())
		{
			continue;
		}
		++residents;
		gold += static_cast<float>(villager->Gold());
		decor += static_cast<float>(villager->Decorations());
		hurt += 1.0f - villager->Health() / villager->MaxHealth();
	}

	life.residents = residents;
	life.prosperity = (residents > 0) ? (gold + 4.0f * decor) / static_cast<float>(residents) : 0.0f;
	life.thefts = std::max(0.0f, life.thefts - 1.0f / 90.0f);   // 도둑질은 한 시간 반쯤 입에 오르내린다
	life.danger = 0.35f * static_cast<float>(village) + 0.5f * life.thefts
	            + ((residents > 0) ? 2.0f * hurt / static_cast<float>(residents) : 0.0f);

	// 이웃 마을에 편지를 쓴다. 전령이 다음 일행 편에 들고 간다.
	life.mailTimer -= 1.0f;
	if (life.mailTimer <= 0.0f)
	{
		life.mailTimer = RandomRange(80.0f, 110.0f);
		if (village > 0 && IsPopulated(village - 1))
		{
			life.mailTo[0] = true;
		}
		if (IsPopulated(village + 1))
		{
			life.mailTo[1] = true;
		}
	}
}

//
// 소식이 닿았다. 이 마을 사람들이 저 마을 이야기를 듣고, 제 성향에 따라
// 마음이 움직인 사람은 이사를 결심한다.
//
//   욕심   × (저 마을이 넉넉한 만큼)
//   겁     × (이 마을이 더 위험한 만큼)
//   방랑기 × 0.5
//   가게 주인과 호위는 제 일이 있어 잘 떠나지 않는다
//
void Game::SpreadNews(int toVillage, int aboutVillage)
{
	auto here = m_Lives.find(toVillage);
	auto there = m_Lives.find(aboutVillage);
	if (here == m_Lives.end() || there == m_Lives.end() || !here->second.populated || !there->second.populated)
	{
		return;
	}

	const Rumor rumor = { aboutVillage, there->second.prosperity, there->second.danger,
	                      static_cast<int>(there->second.thefts + 0.5f) };
	bool replaced = false;
	for (size_t i = 0; i < here->second.rumors.size(); ++i)
	{
		if (here->second.rumors[i].about == aboutVillage)
		{
			here->second.rumors[i] = rumor;
			replaced = true;
		}
	}
	if (!replaced)
	{
		here->second.rumors.push_back(rumor);
	}

	if (there->second.residents >= kVillageCapacity)
	{
		return;   // 갈 자리가 없다
	}

	bool announced = false;
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		VillagerActor* villager = m_Villagers[i];
		if (villager->Village() != toVillage || villager->IsDead() || villager->IsBusy()
		    || villager->IsRescued() || villager->WantsToMove() >= 0)
		{
			continue;
		}

		float desire = villager->Greed() * (there->second.prosperity - here->second.prosperity) / 15.0f
		             + villager->Timidity() * (here->second.danger - there->second.danger)
		             + villager->Wanderlust() * 0.5f;
		if (ProfessionOf(villager->GetProfession()).shop)
		{
			desire -= 0.5f;
		}
		if (villager->EscortStrength() > 0)
		{
			desire -= 0.3f;   // 호위는 길을 오가며 번다
		}

		if (desire > 0.55f && Roll(35))
		{
			villager->SetWantsMove(aboutVillage, 180.0f);
			if (!announced)
			{
				const std::string line = std::to_string(aboutVillage + 1) + "번째 마을이 살기 낫다더군";
				SpawnFloatingText(villager->WorldX(), villager->WorldY() - 62.0f, line, -1, kGold, 12.0f);
				announced = true;
			}
		}
	}
}

// ═══ 일행 ═════════════════════════════════════════════════════════

//
// 1초마다. 이웃 마을로 가려는 사람(이사 · 편지)이 있으면 일행을 꾸린다.
// 그 숲길을 건너기에 넉넉한 호위가 모이고 품삯을 치를 수 있어야 떠난다.
//
void Game::FormTrips()
{
	std::vector<int> villages;
	for (auto& entry : m_Lives)
	{
		if (entry.second.populated)
		{
			villages.push_back(entry.first);
		}
	}

	for (size_t v = 0; v < villages.size(); ++v)
	{
		const int from = villages[v];
		for (int side = 0; side < 2; ++side)
		{
			const int to = (side == 0) ? from - 1 : from + 1;
			if (to < 0 || !IsPopulated(to))
			{
				continue;
			}

			// 이미 이쪽으로 모이는 일행이 있으면 새로 꾸리지 않는다
			bool gathering = false;
			for (size_t t = 0; t < m_Trips.size(); ++t)
			{
				if (m_Trips[t].from == from && m_Trips[t].to == to && !m_Trips[t].returning && m_Trips[t].phase == 0)
				{
					gathering = true;
				}
			}
			if (gathering)
			{
				continue;
			}

			// 떠나려는 사람
			std::vector<VillagerActor*> movers;
			VillagerActor* courier = NULL;
			VillageLife& life = m_Lives[from];
			for (size_t i = 0; i < m_Villagers.size(); ++i)
			{
				VillagerActor* villager = m_Villagers[i];
				if (villager->Village() != from || villager->IsDead() || villager->IsBusy())
				{
					continue;
				}
				if (villager->WantsToMove() == to)
				{
					movers.push_back(villager);
				}
				else if (life.mailTo[side] && courier == NULL
				         && villager->GetProfession() == Profession::Courier)
				{
					courier = villager;
				}
			}
			if (movers.empty() && courier == NULL)
			{
				continue;
			}

			// 호위. 힘이 센 사람부터 넉넉해질 때까지.
			std::vector<VillagerActor*> candidates;
			for (size_t i = 0; i < m_Villagers.size(); ++i)
			{
				VillagerActor* villager = m_Villagers[i];
				if (villager->Village() == from && !villager->IsDead() && !villager->IsBusy()
				    && villager->EscortStrength() > 0 && villager->WantsToMove() < 0
				    && villager->Health() >= villager->MaxHealth() * 0.7f)
				{
					candidates.push_back(villager);
				}
			}
			std::sort(candidates.begin(), candidates.end(),
			          [](const VillagerActor* a, const VillagerActor* b)
			          {
			          	return a->EscortStrength() > b->EscortStrength();
			          });

			const int required = RequiredEscort(from, to);
			std::vector<VillagerActor*> escorts;
			int strength = 0;
			int fee = 0;
			for (size_t i = 0; i < candidates.size() && strength < required; ++i)
			{
				escorts.push_back(candidates[i]);
				strength += candidates[i]->EscortStrength();
				fee += candidates[i]->EscortFee();
			}
			if (strength < required)
			{
				continue;   // 호위가 모자라다. 숲에 들어가지 않고 기다린다
			}

			// 이사 가는 사람끼리 품삯을 나눈다. 전령의 편지 길은 마을이 낸다.
			std::vector<VillagerActor*> paying;
			if (!movers.empty())
			{
				const int share = (fee + static_cast<int>(movers.size()) - 1) / static_cast<int>(movers.size());
				for (size_t i = 0; i < movers.size(); ++i)
				{
					if (movers[i]->Gold() >= share)
					{
						paying.push_back(movers[i]);
					}
				}
			}
			if (paying.empty() && courier == NULL)
			{
				continue;   // 품삯이 모자란 사람은 돈을 더 모은다
			}

			Trip trip;
			trip.id = m_NextTripId++;
			trip.from = from;
			trip.to = to;
			trip.phase = 0;
			trip.timer = 30.0f;   // 이만큼 기다려도 다 안 모이면 모인 사람끼리 떠난다

			// 이사 가는 사람은 저 마을에 새 집을 맡아 둔다
			for (size_t i = 0; i < paying.size(); ++i)
			{
				float doorX = 0.0f;
				float doorY = 0.0f;
				long long key = -1;
				if (!ClaimHouse(to, paying[i]->GetProfession(), doorX, doorY, key))
				{
					paying[i]->ClearWantsMove();
					SpawnFloatingText(paying[i]->WorldX(), paying[i]->WorldY() - 62.0f,
					                  "그 마을엔 빈집이 없다더군", -1, kSoulColor, 12.0f);
					continue;
				}
				trip.passengers.push_back(paying[i]->Id());
				trip.houses.push_back(key);
			}
			if (courier != NULL)
			{
				trip.passengers.push_back(courier->Id());
				trip.houses.push_back(-1);   // 전령은 편지를 전하고 돌아온다
				trip.mail = true;
				life.mailTo[side] = false;
			}
			if (trip.passengers.empty())
			{
				continue;
			}

			// 품삯을 치른다
			const int movingCount = static_cast<int>(trip.passengers.size()) - (trip.mail ? 1 : 0);
			if (movingCount > 0)
			{
				const int share = (fee + movingCount - 1) / movingCount;
				for (size_t i = 0; i < trip.passengers.size(); ++i)
				{
					if (trip.houses[i] != -1)   // -1은 전령(이사 가지 않는다). 집 열쇠는 음수일 수도 있다
					{
						if (VillagerActor* mover = FindVillager(trip.passengers[i]))
						{
							mover->TakeGold(share);
						}
					}
				}
			}
			for (size_t i = 0; i < escorts.size(); ++i)
			{
				escorts[i]->AddGold(escorts[i]->EscortFee());
				trip.escorts.push_back(escorts[i]->Id());
			}

			// 문 안쪽에 모인다
			float gatherX = 0.0f;
			float gatherY = 0.0f;
			GatePoint(from, to, gatherX, gatherY);
			auto gather = [&](VillagerActor* member, bool escort)
			{
				member->JoinTrip(trip.id, escort);
				std::vector<std::pair<float, float> > path;
				if (!FindPath(member->WorldX(), member->WorldY(), gatherX, gatherY, path))
				{
					path.clear();
					path.push_back(std::make_pair(gatherX, gatherY));
				}
				member->SetPath(path);
			};
			for (size_t i = 0; i < trip.passengers.size(); ++i)
			{
				if (VillagerActor* member = FindVillager(trip.passengers[i]))
				{
					gather(member, false);
				}
			}
			for (size_t i = 0; i < escorts.size(); ++i)
			{
				gather(escorts[i], true);
			}

			VillagerActor* first = FindVillager(trip.passengers[0]);
			if (first != NULL)
			{
				const std::string line = (trip.mail && trip.passengers.size() == 1)
					? std::string("편지를 부치러 간다")
					: std::to_string(to + 1) + "번째 마을로 이사 간다";
				SpawnFloatingText(first->WorldX(), first->WorldY() - 62.0f, line, -1, kGold, 13.0f);
			}

			m_Trips.push_back(trip);
		}
	}
}

//
// 매 프레임. 모이는 중인 일행은 다 모였는지 보고, 숲길을 건너는 일행은
// 흩어지지 않게 앞사람을 붙잡아 두고, 닿았는지 본다.
//
void Game::UpdateTrips(float deltaSeconds)
{
	for (size_t t = 0; t < m_Trips.size(); ++t)
	{
		Trip& trip = m_Trips[t];

		// 쓰러진 사람은 일행에서 빠진다. 맡아 둔 새 집은 다시 빈다.
		for (size_t i = 0; i < trip.passengers.size();)
		{
			if (FindVillager(trip.passengers[i]) == NULL)
			{
				if (trip.houses[i] != -1)
				{
					ReleaseHouse(trip.houses[i]);
				}
				trip.passengers.erase(trip.passengers.begin() + static_cast<long>(i));
				trip.houses.erase(trip.houses.begin() + static_cast<long>(i));
			}
			else
			{
				++i;
			}
		}
		for (size_t i = 0; i < trip.escorts.size();)
		{
			if (FindVillager(trip.escorts[i]) == NULL)
			{
				trip.escorts.erase(trip.escorts.begin() + static_cast<long>(i));
			}
			else
			{
				++i;
			}
		}

		std::vector<VillagerActor*> members;
		for (size_t i = 0; i < trip.escorts.size(); ++i)
		{
			members.push_back(FindVillager(trip.escorts[i]));
		}
		for (size_t i = 0; i < trip.passengers.size(); ++i)
		{
			members.push_back(FindVillager(trip.passengers[i]));
		}
		if (members.empty())
		{
			trip.phase = 3;   // 아무도 남지 않았다. 아래에서 지운다
			continue;
		}

		if (trip.phase == 0)
		{
			trip.timer -= deltaSeconds;
			bool gathered = true;
			for (size_t i = 0; i < members.size(); ++i)
			{
				gathered = gathered && members[i]->PathDone();
			}
			if (gathered || trip.timer <= 0.0f)
			{
				float startX = 0.0f, startY = 0.0f, goalX = 0.0f, goalY = 0.0f;
				GatePoint(trip.from, trip.to, startX, startY);
				GatePoint(trip.to, trip.from, goalX, goalY);
				if (!FindPath(startX, startY, goalX, goalY, trip.path))
				{
					trip.path.clear();
					trip.path.push_back(std::make_pair(goalX, goalY));
				}
				for (size_t i = 0; i < members.size(); ++i)
				{
					members[i]->SetPath(trip.path);
				}
				trip.phase = 1;
			}
			continue;
		}

		if (trip.phase != 1)
		{
			continue;
		}

		// 가장 뒤처진 사람보다 세 걸음까지만 앞서 간다
		size_t slowest = static_cast<size_t>(-1);
		bool arrived = true;
		for (size_t i = 0; i < members.size(); ++i)
		{
			slowest = std::min(slowest, members[i]->PathIndex());
			arrived = arrived && members[i]->PathDone();
		}
		for (size_t i = 0; i < members.size(); ++i)
		{
			members[i]->SetPathLimit(slowest + 3);
		}
		if (arrived)
		{
			trip.phase = 2;
			continue;
		}

		// 화면 밖 숲길의 습격. 플레이어가 멀리 있으면 진짜 적이 나오지 않으므로
		// 대신 굴린다. 호위의 힘이 어둠보다 세면 호위가 쫓아내고 현상금을 받는다.
		VillagerActor* leader = members[0];
		if (Distance(leader->WorldX(), leader->WorldY(), m_PlayerX, m_PlayerY) > 1600.0f
		    && BeaconBrightness(leader->WorldX(), leader->WorldY()) <= 0.0f)
		{
			trip.ambush -= deltaSeconds;
			if (trip.ambush <= 0.0f)
			{
				trip.ambush = RandomRange(10.0f, 16.0f);
				const int deeper = std::max(trip.from, trip.to);
				if (Roll(30 + 5 * deeper))
				{
					int strength = 0;
					for (size_t i = 0; i < trip.escorts.size(); ++i)
					{
						if (VillagerActor* escort = FindVillager(trip.escorts[i]))
						{
							strength += escort->EscortStrength();
						}
					}
					const float threat = 1.0f + 0.5f * static_cast<float>(deeper) + RandomRange(0.0f, 1.5f);
					if (static_cast<float>(strength) >= threat)
					{
						for (size_t i = 0; i < trip.escorts.size(); ++i)
						{
							if (VillagerActor* escort = FindVillager(trip.escorts[i]))
							{
								escort->AddGold(Prices::Bounty(false, false) * 2);
							}
						}
					}
					else
					{
						const size_t victim = static_cast<size_t>(RandomRange(0.0f, static_cast<float>(members.size()) - 0.001f));
						members[victim]->Wound(RandomRange(25.0f, 55.0f));
					}
				}
			}
		}
	}

	// 닿은 일행을 마무리한다. 마무리하며 돌아가는 일행이 새로 생기므로 순회가 끝난 뒤에.
	std::vector<Trip> arrived;
	for (size_t t = 0; t < m_Trips.size();)
	{
		if (m_Trips[t].phase >= 2)
		{
			if (m_Trips[t].phase == 2)
			{
				arrived.push_back(m_Trips[t]);
			}
			m_Trips.erase(m_Trips.begin() + static_cast<long>(t));
		}
		else
		{
			++t;
		}
	}
	for (size_t i = 0; i < arrived.size(); ++i)
	{
		FinishTrip(arrived[i]);
	}
}

//
// 일행이 닿았다.
//   · 이사 온 사람은 맡아 둔 새 집으로. 옛 집은 빈다
//   · 편지를 들고 왔으면 이 마을 사람들이 저 마을 소식을 듣는다
//   · 호위와 전령은 제 마을로 돌아간다. 이 마을에서 부칠 편지가 있으면 들고 간다
//   · 돌아온 일행은 각자 집으로
//
void Game::FinishTrip(Trip& trip)
{
	if (trip.returning)
	{
		auto finishMember = [&](int id)
		{
			if (VillagerActor* member = FindVillager(id))
			{
				member->LeaveTrip();
				SendHome(member, member->WorldX(), member->WorldY());
			}
		};
		for (size_t i = 0; i < trip.escorts.size(); ++i)
		{
			finishMember(trip.escorts[i]);
		}
		for (size_t i = 0; i < trip.passengers.size(); ++i)
		{
			finishMember(trip.passengers[i]);
		}
		if (trip.mail)
		{
			SpreadNews(trip.to, trip.from);
		}
		return;
	}

	float centerX = 0.0f;
	float centerY = 0.0f;
	VillageCenter(trip.to, centerX, centerY);

	Trip back;
	back.id = m_NextTripId++;
	back.from = trip.to;
	back.to = trip.from;
	back.returning = true;
	back.phase = 1;

	for (size_t i = 0; i < trip.passengers.size(); ++i)
	{
		VillagerActor* passenger = FindVillager(trip.passengers[i]);
		if (passenger == NULL)
		{
			continue;
		}
		if (trip.houses[i] == -1)
		{
			back.passengers.push_back(passenger->Id());   // 전령은 함께 돌아간다
			back.houses.push_back(-1);
			continue;
		}

		ReleaseHouse(passenger->HouseKey());
		float doorX = centerX;
		float doorY = centerY;
		DoorOf(trip.houses[i], doorX, doorY);
		passenger->Relocate(trip.to, centerX, centerY, doorX, doorY, trip.houses[i]);
		passenger->LeaveTrip();
		SendHome(passenger, passenger->WorldX(), passenger->WorldY());
		SpawnFloatingText(passenger->WorldX(), passenger->WorldY() - 62.0f, "이사 왔소!", -1, kGold, 13.0f);
	}

	if (trip.mail)
	{
		SpreadNews(trip.to, trip.from);
	}

	// 돌아가는 길에 이쪽 마을의 편지를 들고 간다
	auto here = m_Lives.find(trip.to);
	const int side = (trip.from < trip.to) ? 0 : 1;
	if (here != m_Lives.end() && here->second.mailTo[side])
	{
		back.mail = true;
		here->second.mailTo[side] = false;
	}

	for (size_t i = 0; i < trip.escorts.size(); ++i)
	{
		back.escorts.push_back(trip.escorts[i]);
	}
	if (back.escorts.empty() && back.passengers.empty())
	{
		return;
	}

	back.path.assign(trip.path.rbegin(), trip.path.rend());
	auto start = [&](int id, bool escort)
	{
		if (VillagerActor* member = FindVillager(id))
		{
			member->JoinTrip(back.id, escort);
			member->SetPath(back.path);
		}
	};
	for (size_t i = 0; i < back.escorts.size(); ++i)
	{
		start(back.escorts[i], true);
	}
	for (size_t i = 0; i < back.passengers.size(); ++i)
	{
		start(back.passengers[i], false);
	}
	m_Trips.push_back(back);
}

// 플레이어가 맡아 온 편지를 그 마을 사람에게 건넨다.
void Game::DeliverPlayerLetter(VillagerActor* receiver)
{
	if (!m_Letter.carrying || receiver->Village() != m_Letter.to)
	{
		return;
	}
	m_Player->AddGold(Prices::kLetterReward);
	SpreadNews(m_Letter.to, m_Letter.from);
	m_Trade.reply = "편지 잘 받았네. " + std::to_string(m_Letter.from + 1)
	              + "번째 마을 소식이구먼. 수고했네.";
	m_Trade.replyTime = 5.0f;
	m_Letter = LetterState();
}
