#include "stdafx.h"
#include "DrawStats.h"

#include <cstring>

const char* DrawTagName(DrawTag tag)
{
	switch (tag)
	{
	case DrawTag::GroundFog:   return "땅-안개";
	case DrawTag::GroundLight: return "땅-조명";
	case DrawTag::Mist:        return "연무";
	case DrawTag::Canopy:      return "수풀";
	case DrawTag::House:       return "집";
	case DrawTag::Tree:        return "나무";
	case DrawTag::Wall:        return "울타리";
	case DrawTag::Prop:        return "물건";
	case DrawTag::Tower:       return "종탑";
	case DrawTag::Sleeper:     return "굳은사람";
	case DrawTag::Fire:        return "불";
	case DrawTag::Villager:    return "마을사람";
	case DrawTag::Player:      return "플레이어";
	case DrawTag::Companion:   return "동행자";
	case DrawTag::Enemy:       return "적";
	case DrawTag::Projectile:  return "발사체";
	case DrawTag::Orb:         return "구슬";
	case DrawTag::Pickup:      return "보급품";
	case DrawTag::FloatText:   return "뜬글자";
	case DrawTag::Skill:       return "스킬";
	case DrawTag::Hud:         return "HUD";
	case DrawTag::Minimap:     return "미니맵";
	case DrawTag::Dialogue:    return "대화";
	case DrawTag::Post:        return "후처리";
	case DrawTag::Other:       return "기타";
	default:                   return "?";
	}
}

const char* DrawTagKey(DrawTag tag)
{
	switch (tag)
	{
	case DrawTag::GroundFog:   return "ground_fog";
	case DrawTag::GroundLight: return "ground_light";
	case DrawTag::Mist:        return "mist";
	case DrawTag::Canopy:      return "canopy";
	case DrawTag::House:       return "house";
	case DrawTag::Tree:        return "tree";
	case DrawTag::Wall:        return "wall";
	case DrawTag::Prop:        return "prop";
	case DrawTag::Tower:       return "tower";
	case DrawTag::Sleeper:     return "sleeper";
	case DrawTag::Fire:        return "fire";
	case DrawTag::Villager:    return "villager";
	case DrawTag::Player:      return "player";
	case DrawTag::Companion:   return "companion";
	case DrawTag::Enemy:       return "enemy";
	case DrawTag::Projectile:  return "projectile";
	case DrawTag::Orb:         return "orb";
	case DrawTag::Pickup:      return "pickup";
	case DrawTag::FloatText:   return "floattext";
	case DrawTag::Skill:       return "skill";
	case DrawTag::Hud:         return "hud";
	case DrawTag::Minimap:     return "minimap";
	case DrawTag::Dialogue:    return "dialogue";
	case DrawTag::Post:        return "post";
	case DrawTag::Other:       return "other";
	default:                   return "unknown";
	}
}

DrawTag DrawTagFromActorName(const char* name)
{
	if (name == NULL)
	{
		return DrawTag::Other;
	}

	// 액터 이름은 생성자에 박혀 있는 짧은 문자열이다. 장면 그래프가
	// 액터를 붙일 때 한 번만 부르므로 선형 비교로 충분하다.
	struct Entry { const char* name; DrawTag tag; };
	static const Entry kTable[] =
	{
		{ "Ground",     DrawTag::GroundFog },
		{ "House",      DrawTag::House },
		{ "Tree",       DrawTag::Tree },
		{ "Wall",       DrawTag::Wall },
		{ "Prop",       DrawTag::Prop },
		{ "Tower",      DrawTag::Tower },
		{ "Sleeper",    DrawTag::Sleeper },
		{ "Bonfire",    DrawTag::Fire },
		{ "Torch",      DrawTag::Fire },
		{ "Decor",      DrawTag::Prop },
		{ "Villager",   DrawTag::Villager },
		{ "Player",     DrawTag::Player },
		{ "Companion",  DrawTag::Companion },
		{ "Enemy",      DrawTag::Enemy },
		{ "Ember",      DrawTag::Projectile },
		{ "Stone",      DrawTag::Projectile },
		{ "Arrow",      DrawTag::Projectile },
		{ "Bomb",       DrawTag::Projectile },
		{ "Effect",     DrawTag::Skill },
		{ "Zone",       DrawTag::Skill },
		{ "Orb",        DrawTag::Orb },
		{ "Pickup",     DrawTag::Pickup },
		{ "FloatingText", DrawTag::FloatText },
	};

	for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
	{
		if (std::strcmp(name, kTable[i].name) == 0)
		{
			return kTable[i].tag;
		}
	}
	return DrawTag::Other;
}

void DrawSample::Clear()
{
	std::memset(calls, 0, sizeof(calls));
	std::memset(instances, 0, sizeof(instances));
	flushes = 0;
	kindBreaks = 0;
}

int DrawSample::TotalCalls() const
{
	int total = 0;
	for (int i = 0; i < kTagCount; ++i)
	{
		total += calls[i];
	}
	return total;
}

int DrawSample::TotalInstances() const
{
	int total = 0;
	for (int i = 0; i < kTagCount; ++i)
	{
		total += instances[i];
	}
	return total;
}
