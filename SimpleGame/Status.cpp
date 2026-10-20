#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Dialogue.h"
#include "Monitor.h"
#include "Progression.h"
#include "Skills.h"

#include <sstream>

using namespace Art;

//
// 지금 이 순간의 상태를 JSON 한 덩어리로 만든다.
//
// 브라우저의 대시보드가 1초마다 받아 간다. 바깥에서 보기 위한 것이므로
// 게임 규칙은 하나도 들어 있지 않고, 읽기만 한다.
//
// 담는 것은 셋이다.
//   · 플레이어  — 레벨, 체력, 자리, 얻은 특성
//   · 사람들    — 마을 사람 하나하나의 상태와 지금 하는 일
//   · 마을들    — 구역마다 종을 울렸는지, 누가 굳어 있는지, 적이 몇인지
//
namespace
{
	const char* EnemyKindName(EnemyKind kind)
	{
		switch (kind)
		{
		case EnemyKind::Wraith: return "떠도는 그림자";
		case EnemyKind::Husk:   return "삼켜진 자";
		case EnemyKind::Warden: return "종탑지기";
		default:                return "?";
		}
	}
}

std::string Game::BuildStatusJson(float fps) const
{
	std::ostringstream out;

	const int homeX = kHomeRegionX;
	const int homeY = kHomeRegionY;

	out << "{";
	out << "\"time\":" << Json::Number(m_Time);
	out << ",\"fps\":" << Json::Number(fps);

	// ── 플레이어 ──
	{
		const PlayerCombat& stats = Stats();
		out << ",\"player\":{"
		    << "\"level\":" << Json::Number(stats.level)
		    << ",\"health\":" << Json::Number(stats.health)
		    << ",\"maxHealth\":" << Json::Number(m_Player->MaxHealth())
		    << ",\"souls\":" << Json::Number(stats.souls)
		    << ",\"soulsNeeded\":" << Json::Number(Progression::SoulsToNextLevel(stats.level))
		    << ",\"weaponTier\":" << Json::Number(stats.weaponTier)
		    << ",\"gold\":" << Json::Number(stats.gold)
		    << ",\"torches\":" << Json::Number(stats.torches)
		    << ",\"healPacks\":" << Json::Number(stats.healPacks)
		    << ",\"oil\":" << Json::Number(m_Player->Oil())
		    << ",\"x\":" << Json::Number(m_PlayerX)
		    << ",\"y\":" << Json::Number(m_PlayerY)
		    << ",\"region\":[" << Regions::RegionOfTile(FloorToInt(m_PlayerX / kTileSize))
		    << "," << Regions::RegionOfTile(FloorToInt(m_PlayerY / kTileSize)) << "]"
		    << ",\"village\":" << Json::Number(m_Regions.VillageIndex(
		           Regions::RegionOfTile(FloorToInt(m_PlayerX / kTileSize)),
		           Regions::RegionOfTile(FloorToInt(m_PlayerY / kTileSize))))
		    << ",\"respawning\":" << Json::Bool(m_PendingRespawn)
		    << ",\"talking\":" << Json::Bool(m_Dialogue.active)
		    << "}";
	}

	// ── 개수 ──
	{
		int wardens = 0;
		int outskirt = 0;
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			if (m_Enemies[i]->IsWarden())
			{
				++wardens;
			}
			if (m_Enemies[i]->IsOutskirt() && !m_Enemies[i]->IsDying())
			{
				++outskirt;
			}
		}
		int angry = 0;
		for (size_t i = 0; i < m_Villagers.size(); ++i)
		{
			if (m_Villagers[i]->HasGrudge())
			{
				++angry;
			}
		}

		out << ",\"counts\":{"
		    << "\"enemies\":" << Json::Number(static_cast<int>(m_Enemies.size()))
		    << ",\"wardens\":" << Json::Number(wardens)
		    << ",\"outskirt\":" << Json::Number(outskirt)
		    << ",\"angryVillagers\":" << Json::Number(angry)
		    << ",\"villagers\":" << Json::Number(static_cast<int>(m_Villagers.size()))
		    << ",\"actors\":" << Json::Number(static_cast<int>(m_Scene.ActorCount()))
		    << ",\"lightChunks\":" << Json::Number(static_cast<int>(m_Light.LiveChunks()))
		    << ",\"mapChunks\":" << Json::Number(static_cast<int>(m_World.CachedChunks()))
		    << ",\"rungRegions\":" << Json::Number(m_Regions.RungRegionCount())
		    << "}";
	}

	// ── 마을 사람 하나하나 ──
	out << ",\"npcs\":[";
	{
		bool first = true;
		for (size_t i = 0; i < m_Villagers.size(); ++i)
		{
			const VillagerActor* villager = m_Villagers[i];
			if (!first)
			{
				out << ",";
			}
			first = false;

			out << "{\"type\":\"villager\""
			    << ",\"id\":" << Json::Number(villager->Id())
			    << ",\"name\":" << Json::Quote(villager->DisplayName())
			    << ",\"rescued\":" << Json::Bool(villager->IsRescued())
			    << ",\"gold\":" << Json::Number(villager->Gold())
			    << ",\"decorations\":" << Json::Number(villager->Decorations())
			    << ",\"thief\":" << Json::Bool(villager->IsThief())
			    << ",\"stolen\":" << Json::Number(villager->Stolen())
			    << ",\"quarrel\":" << Json::Bool(villager->InQuarrel())
			    << ",\"quarry\":" << Json::Number(villager->Quarry())
			    << ",\"activity\":" << Json::Quote(villager->Activity())
			    << ",\"health\":" << Json::Number(villager->Health())
			    << ",\"maxHealth\":" << Json::Number(villager->MaxHealth())
			    << ",\"grudge\":" << Json::Number(villager->Grudge())
			    << ",\"patrolling\":" << Json::Bool(villager->IsPatrolling())
			    << ",\"x\":" << Json::Number(villager->WorldX())
			    << ",\"y\":" << Json::Number(villager->WorldY())
			    << "}";
		}

		// 동행자도 사람이다. 지금 따라오는 중이면 함께 보낸다.
		if (m_Companion != NULL)
		{
			if (!first)
			{
				out << ",";
			}
			const Skills::Info& skill = Skills::At(m_Companion->Profession());
			out << "{\"type\":\"companion\""
			    << ",\"name\":" << Json::Quote(std::string("동행 · ") + skill.who)
			    << ",\"rescued\":false"
			    << ",\"skill\":" << Json::Quote(skill.name)
			    << ",\"style\":" << Json::Quote(skill.style)
			    << ",\"cooldown\":" << Json::Number(m_Companion->SkillCooldown())
			    << ",\"cooldownMax\":" << Json::Number(skill.cooldown)
			    << ",\"activity\":" << Json::Quote(m_Companion->IsFrozenAgain() ? "다시 굳음" : "따라온다")
			    << ",\"darkness\":" << Json::Number(m_Companion->Darkness())
			    << ",\"x\":" << Json::Number(m_Companion->WorldX())
			    << ",\"y\":" << Json::Number(m_Companion->WorldY())
			    << "}";
		}
	}
	out << "]";

	// ── 마을(구역)마다 ──
	out << ",\"villages\":[";
	{
		for (size_t i = 0; i < m_NearbyRegions.size(); ++i)
		{
			const Regions::Region& region = m_NearbyRegions[i];
			if (i > 0)
			{
				out << ",";
			}

			// 이 구역 안에 있는 적을 센다.
			const float towerX = TileToWorld(region.towerTileX);
			const float towerY = TileToWorld(region.towerTileY);
			const float half = Regions::RegionTileSpan() * kTileSize * 0.5f;

			int enemies = 0;
			for (size_t e = 0; e < m_Enemies.size(); ++e)
			{
				if (m_Enemies[e]->IsDying())
				{
					continue;
				}
				if (std::fabs(m_Enemies[e]->WorldX() - towerX) <= half
				    && std::fabs(m_Enemies[e]->WorldY() - towerY) <= half)
				{
					++enemies;
				}
			}

			const bool isHome = (region.regionX == homeX && region.regionY == homeY);
			const DialogueScript& script = WakeScript(static_cast<int>(region.personSeed));

			out << "{\"region\":[" << region.regionX << "," << region.regionY << "]"
			    << ",\"order\":" << Json::Number(region.village)
			    << ",\"home\":" << Json::Bool(isHome)
			    << ",\"rung\":" << Json::Bool(region.rung)
			    << ",\"woken\":" << Json::Bool(region.woken)
			    << ",\"wardenDefeated\":" << Json::Bool(region.wardenDefeated)
			    << ",\"warmth\":" << Json::Number(region.warmth)
			    << ",\"person\":" << Json::Quote(isHome ? "—" : script.speaker)
			    << ",\"enemies\":" << Json::Number(enemies)
			    << ",\"towerX\":" << Json::Number(towerX)
			    << ",\"towerY\":" << Json::Number(towerY)
			    << ",\"power\":" << Json::Number(PowerAt(towerX, towerY))
			    << "}";
		}
	}
	out << "]";

	// ── 적 ──
	out << ",\"enemies\":[";
	{
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			const EnemyActor* enemy = m_Enemies[i];
			if (i > 0)
			{
				out << ",";
			}
			out << "{\"kind\":" << Json::Quote(EnemyKindName(enemy->Data().kind))
			    << ",\"warden\":" << Json::Bool(enemy->IsWarden())
			    << ",\"health\":" << Json::Number(enemy->Health())
			    << ",\"maxHealth\":" << Json::Number(enemy->MaxHealth())
			    << ",\"dying\":" << Json::Bool(enemy->IsDying())
			    << ",\"outskirt\":" << Json::Bool(enemy->IsOutskirt())
			    << ",\"slowed\":" << Json::Bool(enemy->IsSlowed())
			    << ",\"x\":" << Json::Number(enemy->WorldX())
			    << ",\"y\":" << Json::Number(enemy->WorldY())
			    << "}";
		}
	}
	out << "]";

	// ── 얻은 특성 ──
	out << ",\"perks\":[";
	{
		bool first = true;
		for (int i = 0; i < Perks::Count(); ++i)
		{
			if (!m_Perks.Has(i))
			{
				continue;
			}
			if (!first)
			{
				out << ",";
			}
			first = false;

			const Perks::Info& info = Perks::At(i);
			out << "{\"who\":" << Json::Quote(info.who)
			    << ",\"title\":" << Json::Quote(info.title)
			    << ",\"effect\":" << Json::Quote(info.effect)
			    << "}";
		}
	}
	out << "]";

	out << "}";
	return out.str();
}
