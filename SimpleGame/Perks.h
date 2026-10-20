#pragma once

//
// 되찾은 사람이 남기는 것.
//
// 종을 울려 구역을 되찾으면, 함께 갔던 사람은 첫 마을에 자리를 잡고
// 자기가 하던 일로 플레이어를 돕는다. 대장장이는 무기를 벼리고,
// 약초꾼은 약을 짓고, 종지기 노인은 종 다루는 법을 일러 준다.
//
// 그래서 "사람을 구한다"가 곧 "강해진다"가 된다. 전투로 얻는 레벨과
// 달리 이쪽은 되돌릴 수 없고 누적되므로, 구한 사람 수가 그대로
// 지금까지 해낸 일의 크기가 된다.
//
// 누가 어느 구역에 굳어 있는지는 좌표로 정해져 있다(Regions::personSeed).
// 그 값으로 고르는 대사 번호가 그대로 여기 특성 번호가 된다 —
// 말을 건 사람과 도와주는 사람이 어긋나지 않는다.
//
namespace Perks
{
	// 능력치에 곱하거나 더할 값들. 합류한 사람들의 것을 모두 모은 결과다.
	struct Modifiers
	{
		int   weaponTierBonus = 0;      // 무기 강화 단계를 공짜로 올려 준다
		int   projectileBonus = 0;      // 한 번에 쏘는 불씨 수
		float rangeBonus = 0.0f;        // 사거리(px)
		float pickupBonus = 0.0f;       // 줍는 거리(px)
		float healthBonus = 0.0f;       // 최대 체력
		float soulScale = 1.0f;         // 영혼석에서 얻는 양
		float healScale = 1.0f;         // 회복약과 레벨업 회복량
		float invulnerableBonus = 0.0f; // 맞은 뒤 무적 시간(초)
		float ringScale = 1.0f;         // 종을 울리는 데 걸리는 시간
		float beaconScale = 1.0f;       // 되찾은 종탑이 밝히는 반경
	};

	struct Info
	{
		const char* who;       // 대사의 화자와 같은 이름
		const char* title;     // 특성 이름
		const char* effect;    // 한 줄 설명. 화면에 그대로 나간다
	};

	int Count();
	const Info& At(int index);

	// 대사 번호와 같은 규칙으로 감는다. 음수도 받는다.
	int Wrap(int index);

	//
	// 지금까지 합류한 사람들.
	//
	class Set
	{
	public:
		// 같은 사람이 두 번 합류해도 한 번만 센다.
		// 무한 맵이라 같은 직업이 여러 구역에 굳어 있을 수 있다.
		void Grant(int index);

		bool Has(int index) const;
		int Count() const;

		const Modifiers& Mods() const { return m_Mods; }

		// 가장 최근에 합류한 사람. 화면에 알릴 때 쓴다. 없으면 -1
		int Newest() const { return m_Newest; }

	private:
		void Rebuild();

		unsigned int m_Joined = 0;   // 비트 하나가 사람 하나
		int m_Newest = -1;
		Modifiers m_Mods;
	};
}
