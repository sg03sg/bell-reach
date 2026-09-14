#pragma once

#include <deque>
#include <random>
#include <string>
#include <vector>

#include "Combat.h"
#include "Companion.h"
#include "ExploredMap.h"
#include "LightGrid.h"
#include "Regions.h"
#include "World.h"

class Renderer;
class Input;
struct Color;

//
// 프로토타입의 게임 상태.
//
// 기획서의 한 바퀴를 담는다 —
//   어둠 속을 걸어 굳은 사람을 찾고, 등불을 대고 말을 걸어 깨우고,
//   함께 종탑까지 가서 둘이 종을 울리면 그 구역이 영구히 밝아진다.
//
// 여기에 전투가 얹힌다 —
//   어둠에서 나타나는 적을 불씨로 쓰러뜨리고, 떨어진 영혼석을 주워 레벨을 올리고,
//   종탑을 지키는 종탑지기를 쓰러뜨려야 종을 울릴 수 있다.
//   전투 규칙은 GameCombat.cpp, 성장 공식은 Progression에 있다.
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

	// seed가 같으면 같은 섬이 만들어진다.
	void Initialize(int windowWidth, int windowHeight, unsigned int seed);
	void Update(float deltaSeconds, const Input& input);
	void Render(Renderer* renderer);

	// 후처리가 끝난 화면 위에 따로 그린다. 비네트에 가려지거나 번지지 않게.
	void RenderHud(Renderer* renderer);

	// 콘솔 진단용
	size_t LitChunks() const { return m_Light.LiveChunks(); }
	size_t MapChunks() const { return m_World.CachedChunks(); }

	void ToggleMinimap() { m_ShowMinimap = !m_ShowMinimap; }
	bool IsMinimapVisible() const { return m_ShowMinimap; }
	float PlayerX() const { return m_PlayerX; }
	float PlayerY() const { return m_PlayerY; }
	float Oil() const { return m_Oil; }
	int RungRegions() const { return m_Regions.RungRegionCount(); }
	bool HasCompanion() const { return m_Companion.IsAwake(); }
	bool IsTalking() const { return m_Dialogue.active; }
	float CompanionDarkness() const { return m_Companion.Darkness(); }
	float RingProgress() const { return m_RingProgress; }
	int Level() const { return m_Combat.level; }
	float Health() const { return m_Combat.health; }
	size_t EnemyCount() const { return m_Enemies.size(); }

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

	bool IsBlockedAtWorld(float worldX, float worldY) const;
	float StructureShade(float worldX, float worldY) const;
	void RecordExplored();
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

	// ── 전투 (GameCombat.cpp) ──
	struct PlayerCombat
	{
		int level = 1;
		int souls = 0;            // 이번 레벨에서 모은 영혼
		int weaponTier = 0;
		float health = 100.0f;
		float healthShown = 100.0f;   // 체력바 뒤로 천천히 따라오는 값
		float invulnerable = 0.0f;
		float hurtFlash = 0.0f;
		float magnetTime = 0.0f;
		float attackCooldown = 0.0f;
		float levelUpGlow = 0.0f;
		int deaths = 0;
	};

	void UpdateCombat(float deltaSeconds, const Input& input);
	void SpawnWardens();
	void SpawnEnemies(float deltaSeconds);
	void UpdateEnemies(float deltaSeconds);
	void FireAtEnemies(bool attacking);
	void UpdateProjectiles(float deltaSeconds);
	void UpdateOrbs(float deltaSeconds);
	void UpdatePickups(float deltaSeconds);
	void DamageEnemy(Enemy& enemy, float damage, float fromX, float fromY);
	void KillEnemy(Enemy& enemy);
	void DropLoot(const Enemy& enemy);
	void CollectPickup(const Pickup& pickup);
	void GainSouls(int amount);
	void DamagePlayer(float amount, float fromX, float fromY);
	void RespawnPlayer();

	float PowerAt(float worldX, float worldY) const;
	bool TryMoveBody(float& x, float& y, float dx, float dy, float radius, bool stayOutOfBeacons) const;
	const Enemy* FindEnemy(int id) const;
	float RandomRange(float low, float high);
	bool Roll(int percent);
	void AddFloatingText(float worldX, float worldY, const std::string& label, int number,
	                     const float color[3], float size);

	// 전투 그리기 (Game.cpp)
	void DrawEnemy(Renderer* renderer, const Enemy& enemy);
	void DrawPickups(Renderer* renderer);
	void DrawProjectiles(Renderer* renderer);
	void DrawCombatHud(Renderer* renderer);
	void DrawFloatingTexts(Renderer* renderer);
	float DrawDigits(Renderer* renderer, const std::string& text, float size,
	                 float left, float top, const Color& color);
	float MeasureDigits(Renderer* renderer, const std::string& text, float size);

	// ── 미니맵 ──
	// 밝혀 본 땅은 한 장의 이미지로, 그 위에 있던 것들은 종류별 표식으로 그린다.
	void RebuildMinimap(Renderer* renderer);
	void DrawMinimap(Renderer* renderer);
	void DrawHud(Renderer* renderer);

	float ToRenderX(float worldX) const { return worldX - m_CameraX; }
	float ToRenderY(float worldY) const { return m_CameraY - worldY; }

	int m_WindowWidth = 0;
	int m_WindowHeight = 0;

	float m_CameraX = 0.0f;
	float m_CameraY = 0.0f;

	float m_PlayerX = 0.0f;
	float m_PlayerY = 0.0f;
	float m_PlayerHalfSize = 9.0f;
	float m_FacingX = 1.0f;          // -1 왼쪽, 1 오른쪽. 등불을 드는 손이 바뀐다
	bool m_PlayerMoving = false;

	float m_Time = 0.0f;             // 불꽃 일렁임, 까마귀 몸짓 같은 움직임의 시계

	// 등불. 기름이 줄면 반경도 함께 줄어, 꺼지기 전에 세계가 좁아진다.
	float m_Oil = 1.0f;

	// 남은 화톳불. 적어야 "여기서 쓸까"라는 고민이 산다.
	int m_FiresLeft = 3;

	float m_RingProgress = 0.0f;
	bool m_WardenBlocking = false;   // 종탑지기가 살아 있어 종을 못 울리는 중. 안내 문구에 쓴다
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
	std::vector<World::House> m_VisibleHouses;
	std::vector<World::Tree> m_VisibleTrees;
	std::vector<Drawable> m_Drawables;

	// 전투
	PlayerCombat m_Combat;
	std::vector<Enemy> m_Enemies;
	std::vector<Projectile> m_Projectiles;
	std::vector<Orb> m_Orbs;
	std::vector<Pickup> m_Pickups;
	std::vector<FloatingText> m_FloatingTexts;
	int m_NextEnemyId = 1;
	float m_SpawnTimer = 4.0f;
	bool m_PendingRespawn = false;
	float m_RespawnTimer = 0.0f;
	float m_StartX = 0.0f;           // 되찾은 종탑이 없을 때 다시 일어서는 자리
	float m_StartY = 0.0f;
	std::mt19937 m_Random;

	// 밝혀 본 곳의 기억과 미니맵
	ExploredMap m_Explored;
	bool m_ShowMinimap = true;
	unsigned int m_MapImage = 0;               // 렌더러 텍스처 핸들
	std::vector<unsigned char> m_MapPixels;
	int m_MapOriginX = 0;                      // 지도 이미지 왼쪽 위 칸의 타일 좌표
	int m_MapOriginY = 0;
	unsigned int m_MapVersion = 0;
	float m_MapAge = 0.0f;
	bool m_MapReady = false;
	std::vector<Regions::Region> m_MapRegions;
	std::vector<World::Prop> m_MapProps;

};
