#pragma once

#include <deque>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "Actor.h"
#include "Combat.h"
#include "ExploredMap.h"
#include "LightGrid.h"
#include "Regions.h"
#include "SceneGraph.h"
#include "World.h"

class Renderer;
class Input;
class PlayerActor;
class CompanionActor;
class EnemyActor;
struct Color;

//
// 게임 한 판.
//
// 화면에 놓이는 것은 전부 액터이고, 액터의 순서는 SceneGraph가 맡는다.
// Game이 직접 그리는 것은 화면에 붙는 것들뿐이다 — 체력, 대화창, 미니맵.
//
// Game이 맡는 것은 세계를 굴리는 일이다.
//
//   · 세계의 자료 : World(지형) · Regions(구역) · LightGrid(빛) · ExploredMap(기억)
//   · 액터 관리   : 청크가 가까워지면 지형 액터를 놓고, 멀어지면 치운다
//   · 흐름        : 대화, 종 울리기, 적을 내보내는 일, 쓰러졌다 일어서는 일
//   · 질문 받기   : 액터들이 "여기 밝은가, 막혔는가, 누가 있는가"를 물어 온다
//
// 월드 좌표는 상한이 없고 음수도 유효하다. y축은 아래로 증가한다.
//
class Game
{
public:
	void Initialize(int windowWidth, int windowHeight, unsigned int seed);
	void Update(float deltaSeconds, const Input& input);
	void Render(Renderer* renderer);

	// 후처리가 끝난 화면 위에 그린다. 비네트에 가려지거나 번지지 않게.
	void RenderHud(Renderer* renderer);

	// ── 모니터링 ──
	// 지금 이 순간의 상태를 JSON 한 덩어리로 만든다. 브라우저가 받아 간다.
	std::string BuildStatusJson(float fps) const;

	// ── 진단 ──
	size_t LitChunks() const { return m_Light.LiveChunks(); }
	size_t MapChunks() const { return m_World.CachedChunks(); }
	size_t ActorCount() const { return m_Scene.ActorCount(); }
	size_t EnemyCount() const { return m_Enemies.size(); }
	int Level() const;
	float Health() const;
	float Oil() const;
	int RungRegions() const { return m_Regions.RungRegionCount(); }
	bool HasCompanion() const { return m_Companion != NULL; }
	float CompanionDarkness() const;
	bool IsTalking() const { return m_Dialogue.active; }
	float RingProgress() const { return m_RingProgress; }
	void ToggleMinimap() { m_ShowMinimap = !m_ShowMinimap; }

	// 어둠 위의 연무. 켜고 끄며 차이를 볼 수 있게 한다.
	void ToggleMist() { m_ShowMist = !m_ShowMist; }
	bool IsMistVisible() const { return m_ShowMist; }
	bool IsMinimapVisible() const { return m_ShowMinimap; }

	// ═══ 액터들이 세계에 묻는 것 ═══════════════════════════════════
	const World& GetWorld() const { return m_World; }
	Regions& GetRegions() { return m_Regions; }
	const Regions& GetRegions() const { return m_Regions; }
	const LightGrid& GetLight() const { return m_Light; }

	float PlayerX() const { return m_PlayerX; }
	float PlayerY() const { return m_PlayerY; }

	// 지형
	bool IsBlockedAtWorld(float worldX, float worldY) const;
	bool IsBodyBlocked(float worldX, float worldY, float halfSize) const;
	// 축을 나눠 옮긴다. 벽에 부딪히면 미끄러지고, 원하면 종소리가 닿는 곳을 피한다.
	bool MoveBody(float& x, float& y, float dx, float dy, float radius, bool avoidBeacons) const;

	// 빛
	float BeaconBrightness(float worldX, float worldY) const;
	float TotalBrightness(float worldX, float worldY) const;
	float StructureShade(float worldX, float worldY) const;
	float GlowAt(float worldX, float worldY) const;

	// 흐름
	float PowerAt(float worldX, float worldY) const;
	bool IsRingingAt(int regionX, int regionY) const;
	bool IsTalkingWith(int regionX, int regionY) const;
	bool IsRespawning() const { return m_PendingRespawn; }
	void RequestRespawn();
	void NotifyWardenDefeated(int regionX, int regionY);

	// 적 명부. 자동 조준과 서로 밀어내기가 이 목록을 본다.
	void RegisterEnemy(EnemyActor* enemy);
	void UnregisterEnemy(EnemyActor* enemy);
	const std::vector<EnemyActor*>& Enemies() const { return m_Enemies; }
	EnemyActor* FindEnemy(int id) const;
	int NextEnemyId() { return m_NextEnemyId++; }

	float RandomRange(float low, float high);
	bool Roll(int percent);

	void SpawnFloatingText(float worldX, float worldY, const std::string& label, int number,
	                       const float color[3], float size);

	// 떠오르는 글자는 후처리 뒤에 그려야 또렷하므로 Game이 대신 그린다.
	void DrawFloatingText(const DrawContext& context, const FloatingText& text,
	                      float worldX, float worldY) const;

private:
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
		float revealed = 0.0f;  // 이번 페이지에서 적힌 글자 수
	};

	// 이미 종을 울린 종탑이 주변을 밝히는 범위
	struct Beacon
	{
		float x;
		float y;
	};

	// ── 액터 놓고 치우기 ──
	void StreamActors();
	void LoadChunk(int chunkX, int chunkY);
	void UnloadKey(long long key);
	void LoadRegion(int regionX, int regionY);

	// ── 세계 ──
	void BuildLights();
	void RecordExplored();
	void RefreshNearbyRegions();

	// ── 흐름 ──
	void HandleInteraction(float deltaSeconds, const Input& input);
	void UpdateRinging(float deltaSeconds, bool holding);
	void UpdateSpawning(float deltaSeconds);
	void SpawnWardens();
	void SpawnEnemies(float deltaSeconds);
	void RespawnPlayer();

	// ── 대화 ──
	const Regions::Region* FindTalkableSleeper() const;
	bool TryStartTalk();
	void StartDialogue(const Regions::Region& region);
	void UpdateDialogue(float deltaSeconds, const Input& input);
	void FinishDialogue();

	// ── 화면에 붙는 것들 ──
	void DrawHud(Renderer* renderer);
	void DrawCombatHud(Renderer* renderer);
	void DrawStatus(Renderer* renderer);
	void DrawMist(Renderer* renderer);
	void DrawPerks(Renderer* renderer);
	void DrawDialogue(Renderer* renderer);
	void DrawPrompts(Renderer* renderer);
	void DrawPrompt(Renderer* renderer, const std::string& text, float centerX, float bottomY);
	void RebuildMinimap(Renderer* renderer);
	void DrawMinimap(Renderer* renderer);
	float DrawDigits(Renderer* renderer, const std::string& text, float size,
	                 float left, float top, const Color& color);
	float MeasureDigits(Renderer* renderer, const std::string& text, float size);

	const PlayerCombat& Stats() const;

	// 화면에 붙는 것들이 월드 좌표를 얹을 때 쓴다. 액터는 DrawContext의 것을 쓴다.
	float ToRenderX(float worldX) const { return worldX - m_CameraX; }
	float ToRenderY(float worldY) const { return m_CameraY - worldY; }

	int m_WindowWidth = 0;
	int m_WindowHeight = 0;
	float m_Time = 0.0f;

	float m_CameraX = 0.0f;
	float m_CameraY = 0.0f;

	// 플레이어 액터의 자리를 베껴 둔다. 세계 쪽 코드가 매번 액터를 거치지 않도록.
	float m_PlayerX = 0.0f;
	float m_PlayerY = 0.0f;
	float m_StartX = 0.0f;           // 되찾은 종탑이 없을 때 다시 일어서는 자리
	float m_StartY = 0.0f;

	// ── 장면 ──
	SceneGraph m_Scene;
	PlayerActor* m_Player = NULL;
	CompanionActor* m_Companion = NULL;
	std::vector<EnemyActor*> m_Enemies;
	int m_NextEnemyId = 1;

	// 놓여 있는 청크와 구역. 키가 곧 액터의 StreamKey다.
	std::unordered_set<long long> m_LoadedChunks;
	std::unordered_set<long long> m_LoadedRegions;

	// ── 세계의 자료 ──
	World m_World;
	Regions m_Regions;
	LightGrid m_Light;
	ExploredMap m_Explored;
	std::mt19937 m_Random;

	std::vector<Beacon> m_Beacons;
	std::vector<Regions::Region> m_NearbyRegions;

	// ── 흐름 ──
	float m_RingProgress = 0.0f;
	bool m_RingAvailable = false;
	bool m_WardenBlocking = false;   // 종탑지기가 살아 있어 종을 못 울리는 중
	float m_RingTowerX = 0.0f;
	float m_RingTowerY = 0.0f;
	DialogueState m_Dialogue;
	bool m_PrevInteract = false;
	bool m_PrevPlaceFire = false;
	float m_SpawnTimer = 4.0f;
	bool m_PendingRespawn = false;
	float m_RespawnTimer = 0.0f;

	// ── 상태 창 · 연무 ──
	bool m_ShowStatus = false;           // Tab을 누르고 있는 동안
	bool m_ShowMist = true;
	unsigned int m_MistImage = 0;
	int m_MistColumns = 0;
	int m_MistRows = 0;
	std::vector<unsigned char> m_MistPixels;

	// ── 미니맵 ──
	bool m_ShowMinimap = true;
	unsigned int m_MapImage = 0;
	std::vector<unsigned char> m_MapPixels;
	int m_MapOriginX = 0;
	int m_MapOriginY = 0;
	unsigned int m_MapVersion = 0;
	float m_MapAge = 0.0f;
	bool m_MapReady = false;
	std::vector<Regions::Region> m_MapRegions;
	std::vector<World::Prop> m_MapProps;
};
