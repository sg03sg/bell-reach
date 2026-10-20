#include "stdafx.h"
#include "Game.h"

#include "Actors.h"
#include "Art.h"
#include "Perks.h"
#include "Progression.h"
#include "Renderer.h"
#include "Skills.h"

#include <algorithm>
#include <cstdio>
#include <string>

using namespace Art;

//
// 상태 창 — Tab을 누르고 있는 동안만 뜬다.
//
// 지금 내가 얼마나 강한지, 그 힘이 어디서 왔는지를 한눈에 보여 준다.
// 수치는 모두 전투 코드와 같은 공식(Progression + 되찾은 사람들의 보정)으로
// 계산한다. 화면에 적힌 값과 실제로 나가는 값이 어긋나면 안 되기 때문이다.
//
// 되찾은 사람이 보탠 몫은 값 옆에 금색으로 누가 줬는지 적는다.
// 이 게임에서 강해진다는 것은 곧 누구를 구했는가이므로, 그게 보여야 한다.
//
// 바뀌는 숫자는 DrawDigits로 글자 하나씩 그린다. 문자열을 통째로 그리면
// 값이 바뀔 때마다 글자 텍스처가 새로 쌓인다.
//
namespace
{
	std::string Format(const char* format, float value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), format, value);
		return std::string(text);
	}

	std::string Format(const char* format, int value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), format, value);
		return std::string(text);
	}

	const Color kPanel     = { 0.035f, 0.038f, 0.050f, 0.93f };
	const Color kRim       = { 0.42f, 0.36f, 0.24f, 0.55f };
	const Color kShade     = { 0.0f, 0.0f, 0.0f, 0.38f };
	const Color kRule      = { 0.26f, 0.27f, 0.31f, 0.7f };
	const Color kTitle     = { 0.96f, 0.88f, 0.68f, 1.0f };
	const Color kLabel     = { 0.56f, 0.58f, 0.63f, 1.0f };
	const Color kValue     = { 0.93f, 0.93f, 0.95f, 1.0f };
	const Color kBonus     = { 0.92f, 0.76f, 0.42f, 1.0f };
	const Color kHighlight = { 1.00f, 0.72f, 0.38f, 1.0f };
}

void Game::DrawStatus(Renderer* renderer)
{
	renderer->SetTag(DrawTag::Hud);

	const PlayerCombat& stats = Stats();
	const Perks::Modifiers& mods = Mods();
	const int level = stats.level;

	// ── 전투 코드와 같은 공식 ──
	const int tier = std::min(Progression::kMaxWeaponTier, stats.weaponTier + mods.weaponTierBonus);
	const float damage = Progression::AttackDamage(level, tier);
	const float cooldown = Progression::AttackCooldown(level, tier);
	const int shots = Progression::ProjectileCount(level) + mods.projectileBonus;
	const float range = Progression::AttackRange(level) + mods.rangeBonus;
	const float emberSpeed = Progression::ProjectileSpeed(tier);
	const int pierce = Progression::ProjectilePierce(tier);
	const float perSecond = damage * static_cast<float>(shots) / cooldown;

	const float maxHealth = m_Player->MaxHealth();
	const float oil = m_Player->Oil();
	const float lantern = kLanternRadius * (0.35f + 0.65f * oil);
	const float pickup = Progression::PickupRadius(level) + mods.pickupBonus;
	const float ringTime = kRingSeconds * mods.ringScale;
	const float beacon = BeaconRadius() * mods.beaconScale;

	// ── 크기를 먼저 정한다 ──
	const int perkCount = m_Perks.Count();
	const float rowHeight = 20.0f;
	const float width = 660.0f;
	const float height = 22.0f + 30.0f + 30.0f + 26.0f + 14.0f + 22.0f + rowHeight * 9.0f + 8.0f + 14.0f
	                   + (m_Companion != NULL ? 44.0f : 24.0f) + 14.0f
	                   + 24.0f + 18.0f * static_cast<float>(std::max(1, perkCount)) + 16.0f;

	const float left = -width * 0.5f;
	const float top = height * 0.5f;

	// 뒤를 살짝 가라앉혀 창에 눈이 가게 한다
	renderer->DrawRoundRect(0.0f, 0.0f, static_cast<float>(m_WindowWidth),
	                        static_cast<float>(m_WindowHeight), 0.0f, kShade);
	renderer->DrawRoundRect(0.0f, 0.0f, width + 3.0f, height + 3.0f, 12.0f, kRim);
	renderer->DrawRoundRect(0.0f, 0.0f, width, height, 11.0f, kPanel);

	auto rule = [&](float y)
	{
		renderer->DrawRoundRect(0.0f, y, width - 44.0f, 1.0f, 0.5f, kRule);
	};

	// 한 줄: 이름 · 값 · 단위 · (누가 보탰는가)
	auto row = [&](float columnLeft, float y, const char* label, const std::string& value,
	               const char* unit, const char* bonus, bool highlight)
	{
		renderer->DrawLabel(label, 13.0f, columnLeft, y, kLabel);
		float x = columnLeft + 100.0f;
		x += DrawDigits(renderer, value, 14.0f, x, y, highlight ? kHighlight : kValue) + 3.0f;
		if (unit != NULL && unit[0] != '\0')
		{
			x += renderer->DrawLabel(unit, 11.0f, x, y - 2.0f, kLabel);
		}
		if (bonus != NULL && bonus[0] != '\0')
		{
			renderer->DrawLabel(bonus, 11.0f, x + 6.0f, y - 2.0f, kBonus);
		}
	};

	float y = top - 22.0f;
	const float inner = left + 26.0f;

	// ── 제목 ──
	renderer->DrawLabel("상태", 18.0f, inner, y, kTitle);
	const std::string hint = "Tab을 떼면 닫힙니다";
	renderer->DrawLabel(hint, 11.0f, left + width - 26.0f - renderer->MeasureLabel(hint, 11.0f),
	                    y - 4.0f, kLabel);
	y -= 30.0f;

	// ── 레벨과 영혼 ──
	{
		float x = inner;
		x += DrawDigits(renderer, Format("Lv %d", level), 20.0f, x, y, kTitle) + 18.0f;
		x += renderer->DrawLabel("영혼", 12.0f, x, y - 3.0f, kLabel) + 4.0f;
		const int needed = Progression::SoulsToNextLevel(level);
		x += DrawDigits(renderer, Format("%d", stats.souls) + "/" + Format("%d", needed), 14.0f,
		                x, y - 2.0f, kValue) + 12.0f;

		const float barWidth = 220.0f;
		const float ratio = std::min(1.0f, static_cast<float>(stats.souls) / std::max(1, needed));
		const Color track = { 0.08f, 0.09f, 0.12f, 1.0f };
		const Color fill = { kSoulColor[0], kSoulColor[1], kSoulColor[2], 1.0f };
		renderer->DrawRoundRect(x + barWidth * 0.5f, y - 10.0f, barWidth, 5.0f, 2.5f, track);
		if (ratio > 0.0f)
		{
			renderer->DrawRoundRect(x + barWidth * ratio * 0.5f, y - 10.0f, barWidth * ratio, 5.0f, 2.5f,
			                        fill, 0.0f, 0.6f);
		}
	}
	y -= 30.0f;

	// ── 주머니 ──
	{
		float x = inner;
		renderer->DrawEllipse(x + 5.0f, y - 7.0f, 5.0f, 5.0f, Tint(kGold), 0.0f, 0.6f);
		x += 16.0f;
		x += DrawDigits(renderer, Format("%d", stats.gold), 14.0f, x, y, Tint(kGold, 1.1f)) + 4.0f;
		x += renderer->DrawLabel("골드", 11.0f, x, y - 2.0f, kLabel) + 22.0f;
		x += renderer->DrawLabel("횃불", 12.0f, x, y - 1.0f, kLabel) + 4.0f;
		x += DrawDigits(renderer, Format("%d", stats.torches), 14.0f, x, y, kValue) + 4.0f;
		x += renderer->DrawLabel("R", 11.0f, x, y - 2.0f, kBonus) + 18.0f;
		x += renderer->DrawLabel("회복약", 12.0f, x, y - 1.0f, kLabel) + 4.0f;
		x += DrawDigits(renderer, Format("%d", stats.healPacks), 14.0f, x, y, kValue) + 4.0f;
		renderer->DrawLabel("Q", 11.0f, x, y - 2.0f, kBonus);
	}
	y -= 26.0f;
	rule(y + 6.0f);
	y -= 14.0f;

	// ── 두 칸 ──
	const float leftColumn = inner;
	const float rightColumn = left + width * 0.5f + 12.0f;
	renderer->DrawLabel("몸 · 등불", 12.0f, leftColumn, y, kTitle);
	renderer->DrawLabel("불씨", 12.0f, rightColumn, y, kTitle);
	y -= 22.0f;

	const bool smith  = m_Perks.Has(0);
	const bool miner  = m_Perks.Has(1);
	const bool monk   = m_Perks.Has(2);
	const bool herb   = m_Perks.Has(3);
	const bool hunter = m_Perks.Has(4);
	const bool scribe = m_Perks.Has(5);
	const bool ringer = m_Perks.Has(6);
	const bool soldier = m_Perks.Has(7);

	const float rows = y;

	// 몸 · 등불
	row(leftColumn, y, "체력", Format("%d", static_cast<int>(stats.health + 0.5f)) + "/"
	    + Format("%d", static_cast<int>(maxHealth + 0.5f)), "", monk ? "+25 수도승" : "", false);
	y -= rowHeight;
	row(leftColumn, y, "이동", Format("%.0f", Progression::MoveSpeed(level)), "px/s", "", false);
	y -= rowHeight;
	row(leftColumn, y, "맞은 뒤 무적", Format("%.2f", 0.55f + mods.invulnerableBonus), "초",
	    monk ? "+0.3 수도승" : "", false);
	y -= rowHeight;
	row(leftColumn, y, "영혼석", "x" + Format("%.1f", mods.soulScale), "", miner ? "광부" : "", false);
	y -= rowHeight;
	row(leftColumn, y, "회복량", "x" + Format("%.1f", mods.healScale), "", herb ? "약초꾼" : "", false);
	y -= rowHeight;
	row(leftColumn, y, "기름", Format("%d", static_cast<int>(oil * 100.0f + 0.5f)), "%", "", false);
	y -= rowHeight;
	row(leftColumn, y, "등불 반경", Format("%.0f", lantern), "px", "", false);
	y -= rowHeight;
	row(leftColumn, y, "줍는 거리", Format("%.0f", pickup), "px", scribe ? "+70 필경사" : "", false);
	y -= rowHeight;
	row(leftColumn, y, "종 울리기", Format("%.1f", ringTime), "초", ringer ? "-40% 종지기" : "", false);

	// 불씨
	y = rows;
	row(rightColumn, y, "피해", Format("%.0f", damage), "", "", false);
	y -= rowHeight;
	row(rightColumn, y, "발사 간격", Format("%.2f", cooldown), "초", "", false);
	y -= rowHeight;
	row(rightColumn, y, "한 번에", Format("%d", shots), "발", soldier ? "+1 떠돌이 병사" : "", false);
	y -= rowHeight;
	row(rightColumn, y, "사거리", Format("%.0f", range), "px", hunter ? "+70 사냥꾼" : "", false);
	y -= rowHeight;
	row(rightColumn, y, "불씨 속도", Format("%.0f", emberSpeed), "px/s", "", false);
	y -= rowHeight;
	row(rightColumn, y, "관통", Format("%d", pierce), "명", "", false);
	y -= rowHeight;
	row(rightColumn, y, "무기 강화", Format("%d", tier) + "/" + Format("%d", Progression::kMaxWeaponTier),
	    "단계", smith ? "+1 대장장이" : "", false);
	y -= rowHeight;
	row(rightColumn, y, "종탑 불빛", Format("%.0f", beacon), "px", ringer ? "+25% 종지기" : "", false);
	y -= rowHeight;
	row(rightColumn, y, "초당 피해", Format("%.0f", perSecond), "", "", true);

	y = rows - rowHeight * 9.0f - 8.0f;   // 마지막 줄 글자가 구분선에 걸리지 않게
	rule(y + 6.0f);
	y -= 14.0f;

	// ── 동행자 ──
	if (m_Companion != NULL)
	{
		const Skills::Info& skill = Skills::At(m_Companion->Profession());
		const Color tint = { skill.tint[0], skill.tint[1], skill.tint[2], 1.0f };
		const bool frozen = m_Companion->IsFrozenAgain();

		float x = inner;
		x += renderer->DrawLabel(std::string("동행 · ") + skill.who, 14.0f, x, y, tint) + 10.0f;
		x += renderer->DrawLabel(skill.name, 13.0f, x, y, kValue) + 6.0f;
		x += renderer->DrawLabel(skill.style, 11.0f, x, y - 2.0f, kLabel) + 10.0f;

		if (frozen)
		{
			renderer->DrawLabel("굳어 있다", 11.0f, x, y - 2.0f, kLabel);
		}
		else if (m_Companion->SkillCooldown() <= 0.01f)
		{
			renderer->DrawLabel("준비", 11.0f, x, y - 2.0f, kBonus);
		}
		else
		{
			x += DrawDigits(renderer, Format("%.1f", m_Companion->SkillCooldown()), 12.0f, x, y - 1.0f, kValue);
			renderer->DrawLabel("초", 11.0f, x + 2.0f, y - 2.0f, kLabel);
		}

		renderer->DrawLabel(frozen ? std::string("빛을 다시 비춰 깨워야 함께 싸운다")
		                           : std::string(skill.effect),
		                    11.0f, inner, y - 20.0f, kLabel);
		y -= 44.0f;
	}
	else
	{
		renderer->DrawLabel("동행 없음", 13.0f, inner, y, kLabel);
		y -= 24.0f;
	}
	rule(y + 6.0f);
	y -= 14.0f;

	// ── 되찾은 사람들 ──
	{
		float x = inner;
		x += renderer->DrawLabel("되찾은 사람들", 12.0f, x, y, kTitle) + 6.0f;
		DrawDigits(renderer, Format("%d", perkCount) + "/" + Format("%d", Perks::Count()), 12.0f,
		           x, y, kLabel);
		y -= 24.0f;
	}

	if (perkCount == 0)
	{
		renderer->DrawLabel("아직 아무도 구하지 못했다", 12.0f, inner, y, kLabel);
	}
	for (int i = 0; i < Perks::Count(); ++i)
	{
		if (!m_Perks.Has(i))
		{
			continue;
		}
		const Perks::Info& info = Perks::At(i);
		renderer->DrawLabel(info.who, 12.0f, inner, y, kBonus);
		renderer->DrawLabel(info.title, 12.0f, inner + 110.0f, y, kValue);
		renderer->DrawLabel(info.effect, 11.0f, inner + 250.0f, y - 1.0f, kLabel);
		y -= 18.0f;
	}
}
