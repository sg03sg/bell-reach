#pragma once

#include "DrawStats.h"
#include <memory>
#include <string>
#include <vector>

class Game;
class Input;
class PlayerActor;
class Renderer;
class SceneGraph;

// 액터가 한 프레임 동안 세계에 물어볼 수 있는 것들.
struct ActorContext
{
	float deltaSeconds = 0.0f;
	float time = 0.0f;
	Game* game = NULL;
	SceneGraph* scene = NULL;
	const Input* input = NULL;
	PlayerActor* player = NULL;
};

// 그릴 때 필요한 것들. 월드 좌표를 화면 좌표로 바꾸는 법도 여기 있다.
struct DrawContext
{
	Renderer* renderer = NULL;
	const Game* game = NULL;
	float cameraX = 0.0f;
	float cameraY = 0.0f;
	float time = 0.0f;
	int windowWidth = 0;
	int windowHeight = 0;

	// 월드는 y가 아래로 커지고, 렌더러는 화면 중앙이 원점에 y가 위로 커진다.
	float ToRenderX(float worldX) const { return worldX - cameraX; }
	float ToRenderY(float worldY) const { return cameraY - worldY; }
};

//
// 화면에 놓이는 모든 것의 바탕.
//
// 땅, 집, 나무, 사람, 적, 불씨, 떨어진 물건까지 전부 액터다. 액터는 자기
// 자리(변환)와 자기 행동(OnUpdate)과 자기 모습(OnDraw)만 안다. 서로를 직접
// 부르지 않고 SceneGraph가 순서를 맡는다.
//
// 액터는 다른 액터를 자식으로 달 수 있고, 자식의 자리는 부모를 기준으로 한다.
// 부모가 움직이면 자식도 함께 움직인다 — 종탑에 매달린 종, 사람이 든 등불처럼
// 붙어 다녀야 하는 것들을 위해서다.
//
class Actor
{
public:
	// 그리는 순서를 가르는 층. 같은 층 안에서는 발밑이 아래에 있을수록 앞에 그린다(2.5D).
	enum class Layer
	{
		Ground = 0,    // 땅과 안개
		Object = 1,    // 서 있는 것들 — 집, 나무, 사람, 적
		Effect = 2,    // 날아다니는 것들 — 불씨, 어둠 구슬
		Overlay = 3    // 화면에 겹치는 것들 — 떠오르는 글자
	};

	explicit Actor(const char* name = "Actor");
	virtual ~Actor();

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	// ── 장면 그래프 ──
	Actor* AddChild(std::unique_ptr<Actor> child);
	Actor* Parent() const { return m_Parent; }
	const std::vector<std::unique_ptr<Actor>>& Children() const { return m_Children; }

	// 이번 프레임이 끝나면 자식과 함께 트리에서 빠진다.
	void Destroy() { m_Destroyed = true; }
	bool IsDestroyed() const { return m_Destroyed; }

	// ── 자리 (부모 기준) ──
	void SetPosition(float x, float y) { m_X = x; m_Y = y; }
	void Move(float dx, float dy) { m_X += dx; m_Y += dy; }
	float LocalX() const { return m_X; }
	float LocalY() const { return m_Y; }
	void SetRotation(float radians) { m_Rotation = radians; }
	float LocalRotation() const { return m_Rotation; }
	void SetScale(float scale) { m_Scale = scale; }
	float LocalScale() const { return m_Scale; }

	// ── 자리 (세계 기준) ── 부모까지 누적된 값. SceneGraph가 매 프레임 갱신한다.
	float WorldX() const { return m_WorldX; }
	float WorldY() const { return m_WorldY; }
	float WorldRotation() const { return m_WorldRotation; }
	float WorldScale() const { return m_WorldScale; }

	Layer GetLayer() const { return m_Layer; }
	void SetLayer(Layer layer) { m_Layer = layer; }
	bool IsVisible() const { return m_Visible; }
	void SetVisible(bool visible) { m_Visible = visible; }
	bool IsActive() const { return m_Active; }
	void SetActive(bool active) { m_Active = active; }
	const std::string& Name() const { return m_Name; }

	// 성능 로그에서 어느 칸에 셀지. 이름으로 한 번만 찾아 기억해 둔다.
	DrawTag Tag() const;

	// Game이 청크 단위로 놓고 치울 때 쓰는 꼬리표.
	// 0이면 지형에서 나온 액터가 아니라 스스로 나고 스스로 사라지는 액터다.
	long long StreamKey() const { return m_StreamKey; }
	void SetStreamKey(long long key) { m_StreamKey = key; }

	// 같은 층 안에서의 앞뒤. 기본은 발밑이다.
	virtual float SortDepth() const { return m_WorldY; }

	virtual void OnUpdate(const ActorContext& context) { (void)context; }
	virtual void OnDraw(const DrawContext& context) const { (void)context; }

	// 스스로 빛을 내는 액터는 이것을 재정의한다.
	// 조명은 매 프레임 액터들에게 물어서 세운다 — 등불도 화톳불도 액터다.
	virtual bool GetLight(float& radius, bool& leavesTrail) const
	{
		(void)radius;
		(void)leavesTrail;
		return false;
	}

	// ── SceneGraph만 쓴다 ──
	void ApplyParentTransform(float parentX, float parentY, float parentRotation, float parentScale);
	void PruneDestroyed();

private:
	std::string m_Name;
	mutable int m_Tag = -1;      // 아직 안 찾았으면 -1
	Actor* m_Parent = NULL;
	std::vector<std::unique_ptr<Actor>> m_Children;

	float m_X = 0.0f;
	float m_Y = 0.0f;
	float m_Rotation = 0.0f;
	float m_Scale = 1.0f;

	float m_WorldX = 0.0f;
	float m_WorldY = 0.0f;
	float m_WorldRotation = 0.0f;
	float m_WorldScale = 1.0f;

	Layer m_Layer = Layer::Object;
	bool m_Visible = true;
	bool m_Active = true;
	bool m_Destroyed = false;
	long long m_StreamKey = 0;
};
