#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Professions.h"
#include "Renderer.h"

#include <algorithm>
#include <string>

using namespace Art;

//
// 파티 — 돈을 주고 고용한 모험가들.
//
// 모험가에게 말을 걸어 품삯을 내면 그 사람이 사는 마을의 다음 마을까지 함께
// 간다. 다섯 명까지 데리고 다닐 수 있다. 무엇을 하며 싸우는지는 갈래가
// 정하고(VillagerActor::ServeParty), 여기서는 계약만 맡는다.
//
//   · 계약은 플레이어가 그 다음 마을의 빛(종소리가 닿는 곳) 안에 들어서면 끝난다.
//     아직 어두운 마을이면 종을 울려 밝힐 때까지 곁에 남는다.
//   · 끝나면 그 마을에 빈집이 있으면 거기 눌러앉고, 없으면 제 집으로 돌아간다.
//   · 가방(I)의 파티 칸에서 언제든 돌려보낼 수 있다. 낸 품삯은 돌려받지 못한다.
//   · 숲에서 쓰러지면 그걸로 끝이다. 어둠은 사람을 노린다.
//
// 동행자(종탑까지 데려가는 되찾은 사람)는 파티와 따로다. 다섯 명에 세지 않는다.
//

std::string Game::HireAdventurer(VillagerActor* adventurer)
{
	if (adventurer->IsHired())
	{
		return "이미 함께 다니고 있잖나.";
	}
	if (adventurer->Trip() >= 0 || adventurer->HasPath())
	{
		return "지금은 다른 일행을 호위하는 중이네.";
	}
	if (static_cast<int>(m_Party.size()) >= kMaxParty)
	{
		return "자네 일행이 벌써 다섯이구먼. 더는 길이 비좁아.";
	}
	if (adventurer->Health() < adventurer->MaxHealth() * 0.5f)
	{
		return "몸이 성치 않아. 좀 쉬고 나서 부르게.";
	}

	const int fee = Prices::HireFee(adventurer->GetCalling(), adventurer->Village());
	if (!m_Player->SpendGold(fee))
	{
		return "품삯이 모자라네. " + std::to_string(fee) + "골드야.";
	}
	adventurer->AddGold(fee);

	const int until = adventurer->Village() + 1;
	adventurer->Hire(until, static_cast<int>(m_Party.size()));
	m_Party.push_back(adventurer->Id());

	const CallingInfo& calling = CallingOf(adventurer->GetCalling());
	SpawnFloatingText(adventurer->WorldX(), adventurer->WorldY() - 62.0f, "함께 가지!", -1, calling.tint, 14.0f);
	return std::to_string(until + 1) + "번째 마을까지 함께 가지. 앞장서게.";
}

void Game::UpdateParty()
{
	const int here = VillageOf(m_PlayerX, m_PlayerY);
	const bool lit = BeaconBrightness(m_PlayerX, m_PlayerY) > 0.0f;

	for (size_t i = 0; i < m_Party.size();)
	{
		VillagerActor* member = FindVillager(m_Party[i]);
		if (member == NULL)
		{
			// 쓰러졌다. 알림은 그 사람이 쓰러지는 자리에서 띄웠다(TakeMonsterHit).
			m_Party.erase(m_Party.begin() + static_cast<long>(i));
			continue;
		}
		if (lit && here == member->HiredUntil())
		{
			EndContract(member, true);   // m_Party에서 이 사람을 뺀다
			continue;
		}
		++i;
	}

	// 빠진 자리는 뒷사람이 채운다
	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		if (VillagerActor* member = FindVillager(m_Party[i]))
		{
			member->SetPartySlot(static_cast<int>(i));
		}
	}
}

void Game::EndContract(VillagerActor* member, bool arrived)
{
	const int until = member->HiredUntil();
	member->Dismiss();
	m_Party.erase(std::remove(m_Party.begin(), m_Party.end(), member->Id()), m_Party.end());

	// 데려다준 마을에 사람이 살고 자리가 남았으면 거기서 새 일감을 찾는다.
	// 길 끝에 모험가가 하나둘 남는 것 — 플레이어가 지나간 자리에 마을이 자란다.
	if (arrived && IsPopulated(until))
	{
		auto life = m_Lives.find(until);
		float doorX = 0.0f;
		float doorY = 0.0f;
		long long key = -1;
		if (life != m_Lives.end() && life->second.residents < kVillageCapacity
		    && ClaimHouse(until, member->GetProfession(), doorX, doorY, key))
		{
			ReleaseHouse(member->HouseKey());
			float centerX = 0.0f;
			float centerY = 0.0f;
			VillageCenter(until, centerX, centerY);
			member->Relocate(until, centerX, centerY, doorX, doorY, key);
			SendHome(member, member->WorldX(), member->WorldY());
			SpawnFloatingText(member->WorldX(), member->WorldY() - 62.0f, "여기서 일감을 찾아보지",
			                  -1, kGold, 13.0f);
			return;
		}
	}

	SendHome(member, member->WorldX(), member->WorldY());
	SpawnFloatingText(member->WorldX(), member->WorldY() - 62.0f,
	                  arrived ? "여기까지일세. 또 부르게" : "그럼 돌아가겠네", -1, kGold, 13.0f);
}

void Game::PlaceParty(float worldX, float worldY)
{
	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		if (VillagerActor* member = FindVillager(m_Party[i]))
		{
			const float side = (i % 2 == 0) ? -1.0f : 1.0f;
			member->SetPosition(worldX + side * (30.0f + 16.0f * static_cast<float>(i / 2)),
			                    worldY + 18.0f + 10.0f * static_cast<float>(i));
		}
	}
}

//
// 화면 왼쪽 아래, 동행자 줄 위에 파티가 한 줄씩. 갈래의 빛깔과 체력.
//
void Game::DrawPartyHud(Renderer* renderer)
{
	if (m_Party.empty())
	{
		return;
	}
	renderer->SetTag(DrawTag::Hud);

	const float left = -m_WindowWidth * 0.5f + 18.0f;
	float y = -m_WindowHeight * 0.5f + 100.0f;   // 동행자 줄(64)과 스킬 막대 위
	const Color track = { 0.04f, 0.045f, 0.06f, 0.85f };
	const Color dim = { 0.55f, 0.57f, 0.62f, 0.85f };

	for (size_t i = 0; i < m_Party.size(); ++i)
	{
		const VillagerActor* member = FindVillager(m_Party[i]);
		if (member == NULL)
		{
			continue;
		}
		const CallingInfo& calling = CallingOf(member->GetCalling());
		const Color tint = { calling.tint[0], calling.tint[1], calling.tint[2], 0.95f };

		renderer->DrawRoundRect(left + 4.0f, y - 6.0f, 7.0f, 7.0f, 1.0f, tint, 0.7854f);
		renderer->DrawLabel(calling.name, 12.0f, left + 14.0f, y, tint);

		const float ratio = std::max(0.0f, member->Health() / std::max(1.0f, member->MaxHealth()));
		const float barLeft = left + 66.0f;
		const float barWidth = 70.0f;
		renderer->DrawRoundRect(barLeft + barWidth * 0.5f, y - 6.0f, barWidth, 5.0f, 2.5f, track);
		if (ratio > 0.0f)
		{
			const Color fill = (ratio < 0.35f) ? Color{ 0.85f, 0.25f, 0.2f, 1.0f } : tint;
			renderer->DrawRoundRect(barLeft + barWidth * ratio * 0.5f, y - 6.0f, barWidth * ratio, 5.0f, 2.5f, fill);
		}
		DrawDigits(renderer, std::to_string(member->HiredUntil() + 1), 11.0f, barLeft + barWidth + 8.0f, y, dim);
		renderer->DrawLabel("번째 마을까지", 10.0f,
		                    barLeft + barWidth + 10.0f + MeasureDigits(renderer, std::to_string(member->HiredUntil() + 1), 11.0f),
		                    y - 1.0f, dim);
		y += 17.0f;
	}
}
