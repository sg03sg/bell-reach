#pragma once

//
// 그리기를 어디서 불렀는지 나누는 꼬리표.
//
// 드로우 콜 총합만 보면 "많다"는 것밖에 알 수 없다. 줄이려면 어느 쪽이
// 무거운지 알아야 하므로, 부르는 자리마다 꼬리표를 달아 따로 센다.
//
// 세는 값이 둘인 것에 이유가 있다.
//
//   calls     - glDraw* 를 실제로 부른 횟수. 이것이 드라이버가 받는 명령 수다
//   instances - 화면에 그려진 도형 개수. 모아 그리기를 해도 줄지 않는다
//
// 모아 그리기가 듣고 있는지는 이 둘의 비(instances / calls)로 바로 보인다.
// 1이면 하나씩 그리는 것이고, 수천이면 한 번에 모아 보낸 것이다.
//
enum class DrawTag
{
	// 땅 — 액터 하나가 타일 수천 칸을 그린다
	GroundFog,      // 안개 층. 화면에 보이는 모든 타일
	GroundLight,    // 조명 층. 안개보다 밝은 칸만

	// 지형 액터
	House,
	Tree,
	Prop,
	Tower,
	Sleeper,
	Fire,

	// 살아 있는 것들
	Player,
	Companion,
	Enemy,
	Projectile,
	Orb,
	Pickup,
	FloatText,

	// 화면에 붙는 것
	Hud,
	Minimap,
	Dialogue,

	Post,           // 후처리
	Other,

	Count
};

const char* DrawTagName(DrawTag tag);

// CSV 칸 이름으로 쓸 영문 열쇠. 표 계산기가 한글 열 이름을 싫어하는 일이 있다.
const char* DrawTagKey(DrawTag tag);

// 액터 이름으로 꼬리표를 찾는다. 액터마다 손으로 달지 않아도 되도록,
// 장면 그래프가 액터를 붙일 때 한 번만 찾아 기억해 둔다.
DrawTag DrawTagFromActorName(const char* name);

//
// 한 프레임 동안 꼬리표별로 무슨 일이 있었는가.
//
struct DrawSample
{
	static const int kTagCount = static_cast<int>(DrawTag::Count);

	int calls[kTagCount];
	int instances[kTagCount];

	// 모아 두었다가 한 번에 보낸 횟수. 어느 꼬리표에도 속하지 않는 전체 값이다.
	int flushes;

	// 그리는 종류가 사각형 ↔ 도형으로 바뀌어 끊어 보낸 횟수.
	// 모아 그리기를 막는 것이 바로 이 전환이므로, 이 값이 크면
	// 그리는 순서가 종류별로 뭉쳐 있지 않다는 뜻이다.
	int kindBreaks;

	DrawSample() { Clear(); }

	void Clear();
	int TotalCalls() const;
	int TotalInstances() const;
};
