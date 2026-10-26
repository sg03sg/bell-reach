#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Dialogue.h"
#include "Input.h"
#include "Perks.h"
#include "Professions.h"
#include "Progression.h"
#include "Renderer.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace Art;

//
// 마을의 살림과 거래.
//
// 마을 사람들은 서로를 번호로 부른다(FindVillager). 포인터를 들고 있으면
// 상대가 쓰러져 사라졌을 때 엉뚱한 곳을 가리키게 되기 때문이다.
//
// 플레이어는 Space로 말을 걸어 거래 창을 연다. 열려 있는 동안 세계는 멈추고,
// 숫자 키로 사고, Space로 닫는다. 플레이어가 낸 돈은 파는 사람의 지갑으로
// 가고, 그 사람은 그 돈으로 제 집 둘레를 꾸민다 — 돈을 쓴 만큼 마을이 살아난다.
// 산 물건은 가방(I)에 들고, 단축칸에 걸어 두고 쓴다. 모험가는 물건 대신
// 제 칼(활 · 지팡이 · 방패)을 판다 — 다음 마을까지 함께 가 준다.
//
namespace
{
	const float kTalkReach = 56.0f;   // 이만큼 붙어야 말을 걸 수 있다

	// 월드 좌표의 차이를 방위로 바꾼다. 월드는 y가 아래로 커지므로 북쪽이 -y다.
	const char* Compass(float dx, float dy)
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

	// 받침이 있으면 "이", 없으면 "가". 한글 음절은 UTF-8로 3바이트다.
	const char* SubjectParticle(const std::string& word)
	{
		if (word.size() < 3)
		{
			return "이(가)";
		}
		const unsigned char* tail = reinterpret_cast<const unsigned char*>(word.data() + word.size() - 3);
		if ((tail[0] & 0xF0u) != 0xE0u)
		{
			return "이(가)";
		}
		const unsigned int code = ((tail[0] & 0x0Fu) << 12) | ((tail[1] & 0x3Fu) << 6) | (tail[2] & 0x3Fu);
		if (code < 0xAC00u || code > 0xD7A3u)
		{
			return "이(가)";
		}
		return ((code - 0xAC00u) % 28u != 0u) ? "이" : "가";
	}

	struct Offer
	{
		const char* name;
		const char* note;
		int price;
	};

	// 이 사람이 파는 것. 의사의 치료비는 다친 만큼 달라진다.
	int OffersOf(const VillagerActor* seller, const PlayerActor* player, Offer out[4])
	{
		switch (seller->GetProfession())
		{
		case Profession::Courier:
			out[0] = { "편지 배달", "이웃 마을 사람에게 전하면 15골드", 0 };
			return 1;

		case Profession::Merchant:
			out[0] = { "횃불",   "세워 두면 꺼지지 않는 불. 가방에 든다(R로 바로 세우기)", Prices::kTorch };
			out[1] = { "회복약", "체력 35%. 가방에 든다(Q로 바로 마시기)",               Prices::kHealPack };
			out[2] = { "기름병", "등불 기름 절반. 가방에 든다",                          Prices::kOil };
			return 3;

		case Profession::Doctor:
			out[0] = { "치료", "체력을 다 채운다",
			           Prices::DoctorFee(player->Combat().health, player->MaxHealth()) };
			return 1;

		case Profession::Informant:
			out[0] = { "다음 마을 소식", "어디에 누가 굳어 있는지. 지도에 남는다", Prices::kInfo };
			return 1;

		case Profession::Hunter:
			out[0] = { "강화석", "무기 강화 +1단계. 가방에 든다", Prices::kWeaponStone };
			return 1;

		case Profession::Adventurer:
			out[0] = { "고용 · 다음 마을까지", CallingOf(seller->GetCalling()).note,
			           Prices::HireFee(seller->GetCalling(), seller->Village()) };
			return 1;

		default:
			return 0;
		}
	}
}

// ═══ 마을 사람 찾기 ═══════════════════════════════════════════════

VillagerActor* Game::FindVillager(int id) const
{
	if (id < 0)
	{
		return NULL;
	}
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		if (m_Villagers[i]->Id() == id && !m_Villagers[i]->IsDead())
		{
			return m_Villagers[i];
		}
	}
	return NULL;
}

VillagerActor* Game::FindProfession(Profession profession, int village) const
{
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		VillagerActor* villager = m_Villagers[i];
		if (villager->GetProfession() == profession && villager->Village() == village
		    && !villager->IsRescued() && !villager->IsDead() && villager->Trip() < 0)
		{
			return villager;
		}
	}
	return NULL;
}

VillagerActor* Game::RichestVillagerNear(float worldX, float worldY, float radius, int excludeId, int village) const
{
	// 털 만한 사람. 15골드는 있어야 위험을 무릅쓸 만하다.
	VillagerActor* best = NULL;
	int bestGold = 14;
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		VillagerActor* villager = m_Villagers[i];
		if (villager->Id() == excludeId || villager->IsDead() || villager->Gold() <= bestGold
		    || villager->Village() != village || villager->Trip() >= 0 || villager->IsHired())
		{
			continue;
		}
		if (Distance(worldX, worldY, villager->WorldX(), villager->WorldY()) > radius)
		{
			continue;
		}
		best = villager;
		bestGold = villager->Gold();
	}
	return best;
}

bool Game::GuardNear(float worldX, float worldY, float radius) const
{
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		const VillagerActor* villager = m_Villagers[i];
		if (villager->GetProfession() == Profession::Guard && !villager->IsDead()
		    && Distance(worldX, worldY, villager->WorldX(), villager->WorldY()) <= radius)
		{
			return true;
		}
	}
	return false;
}

void Game::RallyGuards(float worldX, float worldY, float radius, int thiefId, float seconds)
{
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		VillagerActor* villager = m_Villagers[i];
		if (villager->GetProfession() == Profession::Guard && villager->Id() != thiefId
		    && Distance(worldX, worldY, villager->WorldX(), villager->WorldY()) <= radius)
		{
			villager->StartQuarrel(thiefId, seconds);
		}
	}
}

void Game::EndQuarrelsAbout(int villagerId)
{
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		if (m_Villagers[i]->Quarry() == villagerId)
		{
			m_Villagers[i]->EndQuarrel();
		}
	}
}

//
// 집 둘레에 장식을 하나 놓는다. 가게 간판은 집 바로 앞에, 나머지는 둘레에.
// 다른 장식과 너무 붙지 않게 빈자리를 찾는다.
//
void Game::PlaceDecoration(VillagerActor* owner, int index)
{
	const ProfessionInfo& info = ProfessionOf(owner->GetProfession());
	const Decor kind = info.decor[index % kMaxDecorations];
	const float* tint = owner->IsRescued() ? kCloak : info.tint;

	for (int tries = 0; tries < 16; ++tries)
	{
		const float angle = RandomRange(0.0f, 6.2831853f);
		const float reach = (kind == Decor::Sign) ? RandomRange(24.0f, 40.0f) : RandomRange(36.0f, 96.0f);
		const float x = owner->HomeX() + std::cos(angle) * reach;
		const float y = owner->HomeY() + std::sin(angle) * reach * 0.8f;

		if (IsBodyBlocked(x, y, 10.0f))
		{
			continue;
		}

		bool crowded = false;
		m_Scene.ForEach([&](Actor& actor)
		{
			if (actor.Name() == "Decor" && Distance(x, y, actor.WorldX(), actor.WorldY()) < 28.0f)
			{
				crowded = true;
			}
		});
		if (crowded)
		{
			continue;
		}

		m_Scene.Spawn(std::unique_ptr<Actor>(new DecorActor(
			kind, x, y, tint, static_cast<unsigned int>(m_Random()))));
		return;
	}
}

// ═══ 거래 ═════════════════════════════════════════════════════════

VillagerActor* Game::FindTalkableVillager() const
{
	VillagerActor* best = NULL;
	float bestDistance = kTalkReach;
	for (size_t i = 0; i < m_Villagers.size(); ++i)
	{
		VillagerActor* villager = m_Villagers[i];
		// 함께 다니는 모험가는 늘 곁에 있어서, 말을 걸 수 있게 두면 종을 당기려던
		// Space가 거래 창을 연다. 돌려보내는 것은 가방(I)의 파티 칸에서 한다.
		if (villager->IsDead() || villager->IsHired())
		{
			continue;
		}
		const float distance = Distance(m_PlayerX, m_PlayerY, villager->WorldX(), villager->WorldY());
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = villager;
		}
	}
	return best;
}

bool Game::TryStartTrade()
{
	VillagerActor* villager = FindTalkableVillager();
	if (villager == NULL)
	{
		return false;
	}

	m_Trade = TradeState();
	m_Trade.active = true;
	m_Trade.villagerId = villager->Id();

	// 맡아 온 편지가 이 마을로 가는 것이면 말을 거는 것으로 전해진다
	if (m_Letter.carrying && villager->Village() == m_Letter.to)
	{
		DeliverPlayerLetter(villager);
	}
	return true;
}

void Game::UpdateTrade(const Input& input)
{
	VillagerActor* seller = FindVillager(m_Trade.villagerId);

	const bool interact = input.Interact();
	const bool pressed = interact && !m_PrevInteract;
	m_PrevInteract = interact;

	// 닫히는 순간 E · R · Q를 누르고 있었다면 바로 쓰이지 않게
	m_PrevPlaceFire = input.PlaceFire();
	m_PrevTorch = input.PlaceTorch();
	m_PrevHealPack = input.UseHealPack();

	if (seller == NULL || pressed)
	{
		m_Trade.active = false;
		return;
	}

	// 새로 누른 숫자만 산다. 열리는 순간 눌려 있던 숫자는 새로 누른 것이 아니다.
	for (int number = 1; number <= 4; ++number)
	{
		if (KeyPressed(static_cast<unsigned char>('0' + number)))
		{
			Buy(seller, number);
		}
	}
}

void Game::Buy(VillagerActor* seller, int option)
{
	auto reply = [&](const std::string& text)
	{
		m_Trade.reply = text;
		m_Trade.replyTime = 4.0f;
	};

	if (seller->HasGrudge())
	{
		reply("너한테 팔 건 없어!");
		return;
	}

	Offer offers[4];
	const int count = OffersOf(seller, m_Player, offers);
	if (option < 1 || option > count)
	{
		return;
	}
	const Offer& offer = offers[option - 1];

	// 모험가는 물건이 아니라 함께 갈 길을 판다
	if (seller->GetProfession() == Profession::Adventurer)
	{
		reply(HireAdventurer(seller));
		return;
	}

	// 살 까닭이 없는 경우는 돈을 받지 않는다
	if (seller->GetProfession() == Profession::Doctor && offer.price <= 0)
	{
		reply("멀쩡하구먼. 다치면 오게.");
		return;
	}
	if (seller->GetProfession() == Profession::Hunter
	    && Stats().weaponTier + m_Inventory.Count(ItemKind::WeaponStone) >= Progression::kMaxWeaponTier)
	{
		reply("더 벼릴 데가 없네. 가방에 든 것만으로도 충분해.");
		return;
	}

	// 가방에 들어갈 물건이면 자리가 있는지 먼저 본다
	ItemKind bought = ItemKind::None;
	if (seller->GetProfession() == Profession::Merchant)
	{
		bought = (option == 1) ? ItemKind::Torch : (option == 2) ? ItemKind::HealPack : ItemKind::Oil;
	}
	else if (seller->GetProfession() == Profession::Hunter)
	{
		bought = ItemKind::WeaponStone;
	}
	if (bought != ItemKind::None && !m_Inventory.CanAdd(bought))
	{
		reply("가방이 가득 찼구먼. 좀 비우고 오게.");
		return;
	}
	if (seller->GetProfession() == Profession::Informant)
	{
		TellNextVillage(seller);
		return;
	}
	if (seller->GetProfession() == Profession::Courier)
	{
		// 이웃 마을 중 사람이 사는 곳으로 가는 편지를 맡긴다
		if (m_Letter.carrying)
		{
			reply("이미 편지를 들고 있구먼. 그것부터 전해 주게.");
			return;
		}
		const int here = seller->Village();
		int target = -1;
		if (IsPopulated(here + 1))
		{
			target = here + 1;
		}
		else if (here > 0 && IsPopulated(here - 1))
		{
			target = here - 1;
		}
		if (target < 0)
		{
			reply("편지 보낼 곳이 아직 없네. 이웃 마을 종이 울려야 길이 열리지.");
			return;
		}
		m_Letter.carrying = true;
		m_Letter.from = here;
		m_Letter.to = target;
		reply(std::to_string(target + 1) + "번째 마을 사람 아무에게나 전해 주게. 품삯은 거기서 받게.");
		return;
	}

	if (!m_Player->SpendGold(offer.price))
	{
		reply("골드가 모자라네. " + std::to_string(offer.price) + "골드야.");
		return;
	}
	seller->AddGold(offer.price);

	if (bought != ItemKind::None)
	{
		m_Inventory.Add(bought);
	}

	switch (seller->GetProfession())
	{
	case Profession::Merchant:
		if (option == 1)
		{
			reply("꺼지지 않는 불이야. 숲길에 하나씩 세워 두게.");
		}
		else if (option == 2)
		{
			reply("쓴 맛이 나도 참고 마시게.");
		}
		else
		{
			reply("등불이 꺼져 가거든 부어 넣게.");
		}
		break;

	case Profession::Doctor:
		m_Player->Heal(m_Player->MaxHealth());
		reply("이제 됐네. 너무 무리하지 말게.");
		break;

	case Profession::Hunter:
		reply("문 밖 놈들 뼈로 벼린 거야. 가방에 넣어 두었다가 쓰게.");
		break;

	default:
		break;
	}
}

void Game::TellNextVillage(VillagerActor* informant)
{
	auto reply = [&](const std::string& text)
	{
		m_Trade.reply = text;
		m_Trade.replyTime = 6.0f;
	};

	// 아직 종이 울리지 않았고 소식도 듣지 않은 마을 중 가장 앞의 것
	int target = -1;
	int regionX = 0;
	int regionY = 0;
	for (int index = 1; index < 64; ++index)
	{
		m_Regions.VillagePosition(index, regionX, regionY);
		const Regions::Region region = m_Regions.At(regionX, regionY);
		if (!region.rung && !region.heard)
		{
			target = index;
			break;
		}
	}

	if (target < 0)
	{
		reply("더 들려줄 소식이 없네.");
		return;
	}
	if (!m_Player->SpendGold(Prices::kInfo))
	{
		reply("소식은 공짜가 아니지. " + std::to_string(Prices::kInfo) + "골드야.");
		return;
	}
	informant->AddGold(Prices::kInfo);
	m_Regions.MarkHeard(regionX, regionY);

	const Regions::Region region = m_Regions.At(regionX, regionY);
	const std::string who = WakeScript(static_cast<int>(region.personSeed)).speaker;
	const char* direction = Compass(TileToWorld(region.towerTileX) - m_PlayerX,
	                                TileToWorld(region.towerTileY) - m_PlayerY);

	reply(std::to_string(target + 1) + "번째 마을은 " + direction + "이네. "
	      + who + SubjectParticle(who) + " 굳어 있고, 종탑지기가 "
	      + (region.wardenDefeated ? "쓰러졌다더군." : "아직 지키고 있지."));
}

std::string Game::Greeting(const VillagerActor* villager) const
{
	if (villager->HasGrudge())
	{
		return "썩 꺼져!";
	}
	if (villager->Stolen() > 0)
	{
		return "...아, 아무것도 아니야. 가던 길 가게.";
	}
	if (villager->InQuarrel())
	{
		return "도둑놈을 잡아야 해! 비키게!";
	}
	if (villager->IsRescued())
	{
		return std::string(Perks::At(villager->PerkIndex()).title) + " — 덕분에 여기 자리를 잡았네.";
	}

	const int gold = villager->Gold();
	switch (villager->GetProfession())
	{
	case Profession::Merchant:
		return (villager->Decorations() >= 3) ? "요즘 장사가 잘돼서 가게를 좀 꾸몄지."
		                                      : "어둠 속을 걸으려면 불이 있어야지.";
	case Profession::Doctor:
		return "다친 데는 없나?";
	case Profession::Informant:
		return "길 끝에 무엇이 있는지 궁금하지 않나? 소식은 공짜가 아니지만.";
	case Profession::Hunter:
		return (gold >= 30) ? "오늘 현상금이 쏠쏠했어." : "문 밖 놈들 뼈로 쓸 만한 걸 벼려 두었지.";
	case Profession::Guard:
		return "울타리 밖은 우리가 지킨다. 요즘은 안에 도둑이 더 골치지만.";
	case Profession::Knight:
		return (villager->Trip() >= 0) ? "길 위에서는 내 뒤에 붙어 있게."
		                               : "숲길을 건너려거든 나를 부르게. 이사 가는 사람들을 지키는 게 내 일이지.";
	case Profession::Adventurer:
		if (villager->Trip() >= 0)
		{
			return "지금은 다른 일행을 호위하는 중이네.";
		}
		switch (villager->GetCalling())
		{
		case Calling::Warrior:  return "앞에서 베는 건 내게 맡기게. 품삯만 주면 다음 마을까지 가지.";
		case Calling::Archer:   return "멀리 있는 놈부터 떨어뜨려 주지. 다음 마을까지라면.";
		case Calling::Healer:   return "다치면 내가 고쳐 주지. 혼자 숲길은 무리야.";
		case Calling::Mage:     return "무리 지어 오는 놈들은 한 번에 태워 주지.";
		case Calling::Guardian: return "놈들은 내가 붙잡아 두겠네. 그사이에 쏘게.";
		default:
			return (gold >= 30) ? "요즘 호위 일이 쏠쏠해." : "품삯만 맞으면 어느 마을이든 데려다 주지.";
		}
	case Profession::Courier:
		return "이웃 마을에 소식을 전하는 게 내 일이네. 자네가 대신 들고 가 주겠나?";
	default:
		if (villager->WantsToMove() >= 0)
		{
			return std::to_string(villager->WantsToMove() + 1)
			     + "번째 마을로 가려고 짐을 싸는 중이야. 호위만 구하면 떠날 거야.";
		}
		// 이웃 마을에서 온 소식을 전한다
		{
			auto life = m_Lives.find(villager->Village());
			if (life != m_Lives.end() && !life->second.rumors.empty())
			{
				const Rumor& rumor = life->second.rumors[static_cast<size_t>(villager->Id()) % life->second.rumors.size()];
				const std::string where = std::to_string(rumor.about + 1) + "번째 마을은 ";
				if (rumor.thefts >= 2)
				{
					return where + "도둑이 들끓는다더군.";
				}
				if (rumor.prosperity > life->second.prosperity + 10.0f)
				{
					return where + "요즘 살림이 넉넉하다더군. 편지에 그렇게 쓰여 있었어.";
				}
				if (rumor.danger > life->second.danger + 0.5f)
				{
					return where + "어둠이 짙어 무섭다더군.";
				}
			}
		}
		if (gold >= 40)
		{
			return "집 앞에 화분을 하나 더 들일까 해.";
		}
		if (gold < 5)
		{
			return "요즘 먹고살기가 힘들어...";
		}
		return "종소리가 들리는 동안은 살 만하지.";
	}
}

void Game::DrawTrade(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Dialogue);

	const VillagerActor* seller = FindVillager(m_Trade.villagerId);
	if (seller == NULL)
	{
		return;
	}

	Offer offers[4];
	const int count = OffersOf(seller, m_Player, offers);

	// 화면 아래, 대화창과 같은 자리
	const float panelWidth = 860.0f;
	const float panelHeight = 92.0f + 26.0f * static_cast<float>(std::max(1, count));
	const float centerY = -m_WindowHeight * 0.5f + 44.0f + panelHeight * 0.5f;
	const float left = -panelWidth * 0.5f + 30.0f;
	const float top = centerY + panelHeight * 0.5f;

	const ProfessionInfo& info = ProfessionOf(seller->GetProfession());
	const Color rim = { info.tint[0], info.tint[1], info.tint[2], 0.45f };
	const Color panel = { 0.022f, 0.025f, 0.036f, 0.94f };
	renderer->DrawRoundRect(0.0f, centerY, panelWidth + 4.0f, panelHeight + 4.0f, 11.0f, rim);
	renderer->DrawRoundRect(0.0f, centerY, panelWidth, panelHeight, 9.0f, panel);

	// 누구와 이야기하는가, 내 주머니에 얼마 있는가
	const Color nameColor = { info.tint[0] * 1.15f, info.tint[1] * 1.15f, info.tint[2] * 1.15f, 1.0f };
	renderer->DrawRoundRect(left + 3.0f, top - 24.0f, 6.0f, 6.0f, 1.0f, nameColor, 0.7854f);
	renderer->DrawLabel(seller->DisplayName(), 17.0f, left + 16.0f, top - 13.0f, nameColor);

	const std::string purse = std::to_string(Stats().gold);
	const float right = panelWidth * 0.5f - 30.0f;
	const float purseWidth = MeasureDigits(renderer, purse, 15.0f);
	renderer->DrawEllipse(right - purseWidth - 12.0f, top - 22.0f, 6.0f, 6.0f, Tint(kGold), 0.0f, 0.6f);
	DrawDigits(renderer, purse, 15.0f, right - purseWidth, top - 13.0f, Tint(kGold, 1.1f));

	// 그 사람의 말. 방금 무언가를 샀으면 그 대답을, 아니면 인사를.
	const std::string line = (m_Trade.replyTime > 0.0f && !m_Trade.reply.empty()) ? m_Trade.reply
	                                                                                : Greeting(seller);
	const Color speech = { 0.90f, 0.87f, 0.80f, 1.0f };
	renderer->DrawLabel(line, 18.0f, left, top - 42.0f, speech);

	// 파는 것
	const Color number = { 0.96f, 0.86f, 0.62f, 1.0f };
	const Color itemColor = { 0.88f, 0.89f, 0.92f, 1.0f };
	const Color noteColor = { 0.55f, 0.57f, 0.62f, 1.0f };
	float rowY = top - 76.0f;
	for (int i = 0; i < count; ++i)
	{
		const bool afford = Stats().gold >= offers[i].price;
		float x = left + 6.0f;
		x += DrawDigits(renderer, std::to_string(i + 1), 15.0f, x, rowY, number) + 14.0f;
		x += renderer->DrawLabel(offers[i].name, 15.0f, x, rowY, itemColor) + 12.0f;

		if (offers[i].price > 0)
		{
			const Color priceColor = afford ? Tint(kGold, 1.1f) : Color{ 0.62f, 0.32f, 0.28f, 1.0f };
			x += DrawDigits(renderer, std::to_string(offers[i].price), 14.0f, x, rowY - 1.0f, priceColor);
			x += renderer->DrawLabel("골드", 12.0f, x + 2.0f, rowY - 2.0f, noteColor) + 16.0f;
		}
		renderer->DrawLabel(offers[i].note, 12.0f, x, rowY - 2.0f, noteColor);
		rowY -= 26.0f;
	}
	if (count == 0)
	{
		renderer->DrawLabel("파는 것은 없다", 13.0f, left + 6.0f, rowY, noteColor);
	}

	const std::string hint = (seller->GetProfession() == Profession::Adventurer)
		? ("1 고용   Space 닫기   파티 " + std::to_string(m_Party.size()) + "/" + std::to_string(kMaxParty))
		: ((count > 0) ? "숫자로 사기   Space 닫기" : "Space 닫기");
	const float hintWidth = renderer->MeasureLabel(hint, 13.0f);
	renderer->DrawLabel(hint, 13.0f, right - hintWidth, centerY - panelHeight * 0.5f + 26.0f,
	                    Tint(kStone, 1.6f));
}
