#pragma once

#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Actor.h"
#include "Combat.h"
#include "ExploredMap.h"
#include "Items.h"
#include "LightGrid.h"
#include "Perks.h"
#include "Professions.h"
#include "Regions.h"
#include "SceneGraph.h"
#include "World.h"

class Renderer;
class Input;
class PlayerActor;
class CompanionActor;
class EnemyActor;
class VillagerActor;
struct Color;

//
// 게임 한 판.
//
// 화면에 놓이는 것은 전부 액터이고, 액터의 순서는 SceneGraph가 맡는다.
// Game이 직접 그리는 것은 화면에 붙는 것들뿐이다 — 체력, 대화창, 지도, 가방.
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

	// ── 되찾은 사람들이 주는 것 ──
	// 액터들이 능력치를 셀 때 이 값을 얹는다.
	const Perks::Modifiers& Mods() const { return m_Perks.Mods(); }
	const Perks::Set& GetPerks() const { return m_Perks; }
	float CompanionDarkness() const;
	bool IsTalking() const { return m_Dialogue.active; }
	float RingProgress() const { return m_RingProgress; }

	// 어둠 위의 연무. 켜고 끄며 차이를 볼 수 있게 한다.
	void ToggleMist() { m_ShowMist = !m_ShowMist; }
	bool IsMistVisible() const { return m_ShowMist; }

	// 열린 창(큰 지도 · 가방 · 거래)을 하나 닫는다. 닫은 것이 있으면 true — ESC가 쓴다.
	bool CloseOverlay();

	// 고용한 모험가들(마을 사람 번호). 줄 선 차례대로다.
	const std::vector<int>& Party() const { return m_Party; }

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

	// 마을 사람도 과녁이 된다. 적과 목록을 나눠 두는 것은,
	// 자동 조준이 적을 먼저 보고 사람은 적이 없을 때만 보게 하기 위해서다.
	void RegisterVillager(VillagerActor* villager);
	void UnregisterVillager(VillagerActor* villager);
	const std::vector<VillagerActor*>& Villagers() const { return m_Villagers; }

	// 이 둘레의 마을 사람들이 함께 원한을 품는다. 누가 한 사람을 때리면 부른다.
	void RallyVillagers(float worldX, float worldY, float radius, float seconds);

	// ── 마을의 살림(Trade.cpp) ──
	// 마을 사람은 번호로 서로를 부른다. 포인터를 들고 있으면 상대가 사라졌을 때 위험하다.
	int NextVillagerId() { return m_NextVillagerId++; }
	VillagerActor* FindVillager(int id) const;
	VillagerActor* FindProfession(Profession profession, int village) const;
	VillagerActor* RichestVillagerNear(float worldX, float worldY, float radius, int excludeId, int village) const;
	bool GuardNear(float worldX, float worldY, float radius) const;
	void RallyGuards(float worldX, float worldY, float radius, int thiefId, float seconds);
	void EndQuarrelsAbout(int villagerId);

	// owner의 집 둘레에 index번째 장식을 놓는다
	void PlaceDecoration(VillagerActor* owner, int index);

	bool IsTrading() const { return m_Trade.active; }

	// ── 마을들의 삶(Villages.cpp) ──
	// 사람이 사는 마을은 첫 마을과 종을 울린 마을이다. 마을마다 주민 스무 명이
	// 저마다의 집에 살고, 이웃 마을과 편지로 소식을 주고받으며, 소식을 듣고
	// 마음이 움직이면 호위를 사서 이사를 간다.
	bool IsPopulated(int village) const;
	int VillageOf(float worldX, float worldY) const;          // 숲이면 -1
	void VillageCenter(int village, float& worldX, float& worldY) const;
	void NoteTheft(int village);                               // 도둑질은 마을의 소문이 된다

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

	// ── 첫 마을 ──
	// 처음부터 밝고 적이 오지 않는 곳. 되찾아 온 사람들이 여기 자리를 잡는다.
	static const int kHomeRegionX = 0;
	static const int kHomeRegionY = 0;
	static const int kHomeRegionVillage = 0;   // 첫 마을은 길의 0번째 마을
	void SettleHomeVillage();
	// village번째 마을의 빈집을 하나 얻어 그 문 앞에 마을 사람을 놓는다.
	VillagerActor* SpawnVillager(int village, Profession profession, int perkIndex, unsigned int look,
	                             Calling calling = Calling::None);

	// ── 마을들의 삶(Villages.cpp) ──
	struct Rumor
	{
		int about;              // 어느 마을 소식인가
		float prosperity;       // 그 마을이 넉넉한가
		float danger;           // 그 마을이 위험한가
		int thefts;             // 도둑이 들끓는가
	};

	struct VillageLife
	{
		bool populated = false;
		int residents = 0;
		float prosperity = 0.0f;     // 한 사람 몫의 돈 + 장식
		float danger = 0.0f;         // 뒤쪽 마을일수록, 도둑과 다친 사람이 많을수록
		float thefts = 0.0f;         // 최근 도둑질(시간이 지나며 잊힌다)
		float mailTimer = 60.0f;     // 다음 편지를 쓰기까지
		bool mailTo[2] = { false, false };   // [0] 앞 마을로, [1] 다음 마을로 부칠 편지
		std::vector<Rumor> rumors;   // 이웃 마을에서 온 소식
	};

	// 숲길을 건너는 일행
	struct Trip
	{
		int id = 0;
		int from = 0;
		int to = 0;
		std::vector<int> passengers;   // 이사 가는 사람, 편지를 든 전령
		std::vector<int> escorts;      // 기사 · 모험가 · 사냥꾼
		std::vector<long long> houses; // 이사 가는 사람마다 맡아 둔 새 집(passengers와 같은 순서)
		bool mail = false;             // 편지를 들고 가는가
		bool returning = false;        // 호위가 제 마을로 돌아가는 길인가
		int phase = 0;                 // 0 모이는 중, 1 숲길을 건너는 중
		float timer = 0.0f;
		float ambush = 12.0f;          // 화면 밖에서 습격을 굴리기까지
		std::vector<std::pair<float, float> > path;
	};

	void PopulateVillage(int village);
	bool ClaimHouse(int village, Profession profession, float& doorX, float& doorY, long long& key);
	void ReleaseHouse(long long key);
	void DoorOf(long long key, float& doorX, float& doorY) const;
	void UpdateVillages(float deltaSeconds);
	void UpdateVillageLife(int village, VillageLife& life);
	void SpreadNews(int toVillage, int aboutVillage);
	void FormTrips();
	void UpdateTrips(float deltaSeconds);
	void FinishTrip(Trip& trip);
	bool GatePoint(int village, int toward, float& worldX, float& worldY) const;
	bool FindPath(float fromX, float fromY, float toX, float toY,
	              std::vector<std::pair<float, float> >& out) const;
	void SendHome(VillagerActor* villager, float fromX, float fromY);
	void DeliverPlayerLetter(VillagerActor* receiver);
	int NearestPopulatedVillage() const;

	// ── 파티(Party.cpp) ──
	// 모험가를 다음 마을까지 고용한다. 그 사람이 하는 대답을 돌려준다.
	std::string HireAdventurer(VillagerActor* adventurer);
	void UpdateParty();
	// 계약이 끝났다. arrived면 데려다준 마을에 자리가 있으면 거기 머문다. 아니면 집으로 간다.
	void EndContract(VillagerActor* member, bool arrived);
	void PlaceParty(float worldX, float worldY);   // 쓰러졌다 일어선 자리로 함께 옮긴다
	void DrawPartyHud(Renderer* renderer);

	// ── 가방 · 단축칸(InventoryUI.cpp) ──
	bool UseItem(ItemKind kind);
	void UseHotkeys();
	void UpdateInventory();
	void DrawHotbar(Renderer* renderer);
	void DrawInventory(Renderer* renderer);
	void DrawItemNote(Renderer* renderer);
	void NoteItem(const std::string& text);
	int BagSlotAt(float x, float y) const;
	int HotbarSlotAt(float x, float y) const;
	int PartyButtonAt(float x, float y) const;

	// ── 지도(MapView.cpp) ──
	// 밝혀 본 칸을 기억하고 지도 그림에 칠한다.
	void MarkSeen(int tileX, int tileY);
	void PaintMapTile(int tileX, int tileY);
	// 종이 울려 종지기가 있는 마을은 지도에 통째로 밝힌다.
	void RevealVillage(int village);
	// 구역마다 한 장인 지도 그림을 화면에 깐다. scale은 월드 1px이 화면 몇 px인가.
	void DrawMapSheets(Renderer* renderer, float worldCenterX, float worldCenterY, float scale,
	                   float screenCenterX, float screenCenterY, float halfWidth, float halfHeight);
	void UpdateBigMap();
	void DrawBigMap(Renderer* renderer);

	// ── 입력 ──
	// 매 프레임 처음에 한 번, 이번 프레임에 새로 눌린 키와 단추를 잰다.
	void TrackInput(const Input& input);
	bool KeyPressed(unsigned char key) const { return m_KeyEdge[key]; }

	// ── 거래(Trade.cpp) ──
	VillagerActor* FindTalkableVillager() const;
	bool TryStartTrade();
	void UpdateTrade(const Input& input);
	void Buy(VillagerActor* seller, int option);
	void TellNextVillage(VillagerActor* informant);
	void DrawTrade(Renderer* renderer);
	std::string Greeting(const VillagerActor* villager) const;
	bool IsHomeRegion(int regionX, int regionY) const
	{
		return regionX == kHomeRegionX && regionY == kHomeRegionY;
	}

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
	void SpawnOutskirts(float deltaSeconds);
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
	std::vector<VillagerActor*> m_Villagers;
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

	// 마을 사람과의 거래. 열려 있는 동안 세계가 멈춘다(대화처럼).
	struct TradeState
	{
		bool active = false;
		int villagerId = -1;
		std::string reply;        // 산 뒤나 못 샀을 때 그 사람이 하는 말
		float replyTime = 0.0f;
	};
	TradeState m_Trade;

	// 마을들의 삶
	std::unordered_map<int, VillageLife> m_Lives;
	std::vector<Trip> m_Trips;
	int m_NextTripId = 1;
	std::unordered_map<long long, World::House> m_Houses;   // 누군가 사는 집(열쇠 → 집)
	float m_VillageTick = 0.0f;

	// 플레이어가 맡은 편지
	struct LetterState
	{
		bool carrying = false;
		int from = -1;
		int to = -1;
	};
	LetterState m_Letter;
	bool m_PrevTorch = false;
	bool m_PrevHealPack = false;
	int m_NextVillagerId = 1;
	Perks::Set m_Perks;
	float m_PerkNotice = 0.0f;      // 방금 합류한 사람을 화면에 알리는 시간
	int m_PerkNoticeIndex = -1;
	bool m_PrevInteract = false;
	bool m_PrevPlaceFire = false;
	float m_SpawnTimer = 4.0f;
	float m_OutskirtTimer = 3.0f;    // 첫 마을 외곽에 어둠이 몰려오기까지
	bool m_PendingRespawn = false;
	float m_RespawnTimer = 0.0f;

	// ── 상태 창 · 연무 ──
	bool m_ShowStatus = false;           // Tab을 누르고 있는 동안
	bool m_ShowMist = true;
	unsigned int m_MistImage = 0;
	int m_MistColumns = 0;
	int m_MistRows = 0;
	std::vector<unsigned char> m_MistPixels;

	// ── 입력 ──
	bool m_KeyHeld[256] = {};
	bool m_KeyEdge[256] = {};
	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	bool m_MouseHeld[2] = { false, false };
	bool m_MousePressed[2] = { false, false };    // 이번 프레임에 눌렀다
	bool m_MouseReleased[2] = { false, false };   // 이번 프레임에 뗐다
	float m_WheelSeen = 0.0f;
	float m_WheelDelta = 0.0f;

	// ── 가방 · 단축칸 ──
	Inventory m_Inventory;
	bool m_InventoryOpen = false;
	struct DragState
	{
		bool active = false;
		bool fromHotbar = false;   // 단축칸에서 끌었는가(아니면 가방 칸)
		int index = -1;
		ItemKind kind = ItemKind::None;
		float startX = 0.0f;
		float startY = 0.0f;
		bool moved = false;        // 조금이라도 끌었는가. 아니면 그냥 누른 것이다
	};
	DragState m_Drag;
	int m_LastClickSlot = -1;      // 두 번 눌러 쓰기
	float m_LastClickTime = -10.0f;
	std::string m_ItemNote;        // 방금 쓴 것이나 못 쓴 까닭
	float m_ItemNoteTime = 0.0f;

	// ── 파티 ──
	std::vector<int> m_Party;

	// ── 지도 ──
	// 구역마다 한 장. 밝혀 본 칸만 색이 있고 나머지는 투명하다(그 뒤의 검은 바탕이 보인다).
	struct MapSheet
	{
		std::vector<unsigned char> pixels;   // RGBA, 0행이 구역의 맨 위
		unsigned int image = 0;
		bool dirty = true;
	};
	std::unordered_map<long long, MapSheet> m_MapSheets;
	float m_MapUploadAge = 0.0f;   // 칠한 것을 그림으로 올린 뒤 지난 시간

	struct BigMapState
	{
		bool open = false;
		float centerX = 0.0f;      // 화면 가운데에 오는 월드 좌표
		float centerY = 0.0f;
		float scale = 0.05f;       // 월드 1px이 화면 몇 px인가
		bool dragging = false;
		float grabX = 0.0f;
		float grabY = 0.0f;
		float grabCenterX = 0.0f;
		float grabCenterY = 0.0f;
	};
	BigMapState m_BigMap;
	std::vector<Regions::Region> m_MapRegions;
	std::vector<World::Prop> m_MapProps;
};
