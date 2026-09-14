#pragma once

#include <deque>
#include <string>
#include <vector>

#include "Companion.h"
#include "LightGrid.h"
#include "Regions.h"
#include "World.h"

class Renderer;
class Input;

//
// 프로토타입의 게임 상태.
//
// 기획서의 한 바퀴를 담는다 —
//   어둠 속을 걸어 굳은 사람을 찾고, 등불을 대고 말을 걸어 깨우고,
//   함께 종탑까지 가서 둘이 종을 울리면 그 구역이 영구히 밝아진다.
//
// 전투는 아직 없다. 기획서에서 미확정 항목이기 때문이다.
//
// 월드 좌표는 상한이 없고 음수도 유효하다. y축은 아래로 증가한다.
// 카메라가 화면 중심의 월드 좌표를 들고 있고, 렌더러는 화면 중앙이 원점에
// y축이 위로 증가하므로, 그리기 직전에만 변환한다.
//
class Game
{
public:
	struct StaticLight
	{
		float x;
		float y;
		float radius;
	};

	void Initialize(int windowWidth, int windowHeight);
	void Update(float deltaSeconds, const Input& input);
	void Render(Renderer* renderer);

	// 후처리가 끝난 화면 위에 따로 그린다. 비네트에 가려지거나 번지지 않게.
	void RenderHud(Renderer* renderer);

	// 콘솔 진단용
	size_t LitChunks() const { return m_Light.LiveChunks(); }
	size_t MapChunks() const { return m_World.CachedChunks(); }
	float PlayerX() const { return m_PlayerX; }
	float PlayerY() const { return m_PlayerY; }
	float Oil() const { return m_Oil; }
	int RungRegions() const { return m_Regions.RungRegionCount(); }
	bool HasCompanion() const { return m_Companion.IsAwake(); }
	bool IsTalking() const { return m_Dialogue.active; }
	float CompanionDarkness() const { return m_Companion.Darkness(); }
	float RingProgress() const { return m_RingProgress; }

private:
	// 플레이어가 지나온 궤적의 한 점. travelled는 출발점부터의 누적 거리다.
	// 동행자는 이 궤적 위에서 일정 거리만큼 뒤처진 점을 목표로 삼는다.
	struct PathPoint
	{
		float x;
		float y;
		float travelled;
	};

	// 한 프레임에 그릴 오브젝트. 발밑 y로 정렬해 뒤에서 앞으로 그린다(2.5D).
	struct Drawable
	{
		float sortY;
		int kind;
		int index;
	};

	struct DialoguePage
	{
		std::string speaker;
		std::string text;       // '\n'으로 줄을 나눈다
		bool fromPlayer;
	};

	struct DialogueState
	{
		bool active = false;
		int regionX = 0;
		int regionY = 0;
		std::vector<DialoguePage> pages;
		size_t page = 0;
		float revealed = 0.0f;  // 이번 페이지에서 적힌 글자 수. 한 글자씩 늘어난다
	};

	// 이미 종을 울린 구역이 주변을 밝히는 범위. 종소리가 닿는 곳까지다.
	struct Beacon
	{
		float x;
		float y;
	};

	bool IsWallAtWorld(float worldX, float worldY) const;
	bool IsPlayerBlocked(float worldX, float worldY) const;

	void MovePlayer(float deltaSeconds, const Input& input);
	void RecordPath();
	bool CompanionTarget(float& outX, float& outY) const;

	void RefreshNearbyRegions();
	float BeaconBrightness(float worldX, float worldY) const;
	float TotalBrightness(float worldX, float worldY) const;

	void BuildLights();
	void UpdateSleepers(float deltaSeconds);
	void HandleInteraction(float deltaSeconds, const Input& input);
	void UpdateRinging(float deltaSeconds, bool holding);

	// ── 대화 ──
	// 생기가 다 돈 사람에게 말을 걸면 대화가 시작되고, 대화가 끝나야 따라나선다.
	const Regions::Region* FindTalkableSleeper() const;
	bool TryStartTalk();
	void StartDialogue(const Regions::Region& region);
	void UpdateDialogue(float deltaSeconds, const Input& input);
	void FinishDialogue();
	void DrawDialogue(Renderer* renderer);
	void DrawPrompts(Renderer* renderer);
	void DrawPrompt(Renderer* renderer, const std::string& text, float centerX, float bottomY);

	// 오브젝트는 전부 발밑이 기준점이고 화면 위쪽으로 자란다.
	void DrawTower(Renderer* renderer, const Regions::Region& region);
	void DrawSleeper(Renderer* renderer, const Regions::Region& region);
	void DrawCompanion(Renderer* renderer);
	void DrawPlayer(Renderer* renderer);
	void DrawHud(Renderer* renderer);

	float ToRenderX(float worldX) const { return worldX - m_CameraX; }
	float ToRenderY(float worldY) const { return m_CameraY - worldY; }

	int m_WindowWidth = 0;
	int m_WindowHeight = 0;

	float m_CameraX = 0.0f;
	float m_CameraY = 0.0f;

	float m_PlayerX = 0.0f;
	float m_PlayerY = 0.0f;
	float m_PlayerSpeed = 190.0f;    // 픽셀/초
	float m_PlayerHalfSize = 9.0f;
	float m_FacingX = 1.0f;          // -1 왼쪽, 1 오른쪽. 등불을 드는 손이 바뀐다
	bool m_PlayerMoving = false;

	float m_Time = 0.0f;             // 불꽃 일렁임, 까마귀 몸짓 같은 움직임의 시계

	// 등불. 기름이 줄면 반경도 함께 줄어, 꺼지기 전에 세계가 좁아진다.
	float m_Oil = 1.0f;

	// 남은 화톳불. 적어야 "여기서 쓸까"라는 고민이 산다.
	int m_FiresLeft = 3;

	float m_RingProgress = 0.0f;
	bool m_RingAvailable = false;    // 지금 종을 당길 수 있는가. 안내 문구에 쓴다
	float m_RingTowerX = 0.0f;
	float m_RingTowerY = 0.0f;

	DialogueState m_Dialogue;
	bool m_PrevInteract = false;
	bool m_PrevPlaceFire = false;

	World m_World;
	Regions m_Regions;
	LightGrid m_Light;
	Companion m_Companion;

	std::deque<PathPoint> m_PlayerPath;
	float m_PathLength = 0.0f;

	std::vector<StaticLight> m_Fires;          // 설치한 화톳불
	std::vector<Beacon> m_Beacons;             // 이미 울린 종탑
	std::vector<Regions::Region> m_NearbyRegions;
	std::vector<World::Prop> m_VisibleProps;   // 프레임마다 재사용해 할당을 피한다
	std::vector<Drawable> m_Drawables;

};
