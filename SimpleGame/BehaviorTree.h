#pragma once

#include <memory>
#include <string>
#include <vector>

struct ActorContext;
class Actor;

//
// 행동 트리 — 마을 사람이 무엇을 할지 정하는 방식.
//
// if문을 길게 늘어놓는 대신, 할 일을 나무로 쌓는다. 가지 하나를 바꿔도
// 나머지가 흔들리지 않고, 새 행동을 끼워 넣기 쉽다.
//
// 매 프레임 뿌리부터 훑어 내려가며 각 마디가 셋 중 하나를 돌려준다.
//
//   성공(Success) - 할 일을 마쳤다
//   실패(Failure) - 할 수 없다. 다른 가지를 보라
//   진행(Running) - 아직 하는 중이다. 다음 프레임에 이어서
//
// 마디는 네 가지다.
//
//   Selector  - 자식을 왼쪽부터 시도해 처음 성공/진행하는 것을 고른다 (= 또는)
//   Sequence  - 자식을 왼쪽부터 차례로 해내야 한다. 하나라도 실패하면 실패 (= 그리고)
//               매 프레임 처음부터 다시 훑으므로 앞의 조건이 계속 검사된다
//   Condition - 조건을 보고 성공/실패만 돌려준다. 시간은 쓰지 않는다
//   Action    - 실제로 무언가 한다. 여러 프레임에 걸칠 수 있다(진행)
//
// 마을 사람의 나무는 이렇게 생겼다.
//
//   Selector
//    ├ Sequence ── [원한이 있나?]         → [원수를 쫓으며 돌을 던진다]
//    ├ Sequence ── [적이 보이나?]         → [다가가 돌을 던진다]
//    ├ Sequence ── [플레이어가 가까운가?] → [멈춰서 쳐다본다]
//    ├ Sequence ── [쉬는 중인가?]         → [잠시 서 있는다 / 경계한다]
//    ├ Sequence ── [갈 곳이 있나?]        → [걷는다 / 외곽을 순찰한다]
//    └ Action ───── [다음 할 일을 고른다]
//
// 위에 있는 가지가 늘 이긴다. 그래서 순찰하다가도 적이 보이면 싸우고,
// 싸우다가도 누가 때리면 그쪽으로 돌아선다. 우선순위가 트리의 모양 그 자체다.
//
namespace BT
{
	enum class Status
	{
		Success,
		Failure,
		Running
	};

	// 나무가 들여다보는 값들. 마디끼리 주고받는 메모장이다.
	// 액터가 자기 상태를 여기 적어 두면 조건 마디가 읽는다.
	struct Blackboard
	{
		Actor* self = NULL;
		const ActorContext* context = NULL;

		// 지금 향하는 곳
		float targetX = 0.0f;
		float targetY = 0.0f;
		bool hasTarget = false;

		// 남은 시간. 쉬거나 쳐다보는 동안 줄어든다
		float timer = 0.0f;

		// 플레이어와의 거리. 조건 마디가 매번 다시 재지 않도록 미리 적어 둔다
		float playerDistance = 0.0f;

		// 이번에 무엇을 하고 있는지. 화면에 표시하거나 로그로 보기 위한 것
		const char* activity = "";

		// 걷는 속도와 집에서 벗어날 수 있는 거리
		float homeX = 0.0f;
		float homeY = 0.0f;
		float wanderRadius = 220.0f;
		float walkSpeed = 52.0f;

		// ── 마을을 지키는 일 ──
		// 마을 한가운데. 순찰도 싸움도 여기서 leash 너머로는 가지 않는다.
		float villageX = 0.0f;
		float villageY = 0.0f;
		float leash = 790.0f;
		bool patrolling = false;      // 지금 가는 곳이 외곽 순찰 지점인가
		float patrolChance = 30.0f;   // 다음 할 일을 고를 때 순찰을 고를 확률(%)

		// 노리는 것. 매 프레임 액터가 새로 찾아 적어 둔다.
		Actor* foe = NULL;            // 가장 가까운 적. 없으면 NULL
		float foeX = 0.0f;
		float foeY = 0.0f;            // 몸 가운데(맞히는 자리)
		float foeDistance = 0.0f;

		// 원한. 0보다 크면 플레이어를 노린다. 시간이 지나면 식는다.
		float grudge = 0.0f;

		// ── 마을의 살림 ──
		int errand = 0;               // 목적지에 닿으면 할 일(VillagerActor::Errand)
		int errandWith = -1;          // 심부름 상대(마을 사람 번호). 장보기 · 치료 · 도둑질
		int quarry = -1;              // 다투는 상대(마을 사람 번호)
		float quarrel = 0.0f;         // 다툼이 남은 시간
		bool fleeing = false;         // 훔치고 집으로 달아나는 중

		// 던지기
		float attackCooldown = 0.0f;
		float attackEvery = 1.7f;     // 다시 던지기까지(초)
		float attackRange = 250.0f;   // 돌이 닿는 거리
		float noticeRange = 380.0f;   // 적을 알아보는 거리
	};

	class Node
	{
	public:
		virtual ~Node() {}
		virtual Status Tick(Blackboard& board) = 0;

		// 가지가 중간에 버려졌을 때 상태를 되돌린다.
		virtual void Reset() {}
	};

	typedef std::unique_ptr<Node> NodePtr;

	// ── 가지 마디 ──
	class Composite : public Node
	{
	public:
		void Add(NodePtr child) { m_Children.push_back(std::move(child)); }
		void Reset() override;

	protected:
		std::vector<NodePtr> m_Children;
		size_t m_Current = 0;
	};

	// 하나라도 되면 된다.
	class Selector : public Composite
	{
	public:
		Status Tick(Blackboard& board) override;
	};

	// 전부 차례로 되어야 한다.
	class Sequence : public Composite
	{
	public:
		Status Tick(Blackboard& board) override;
	};

	// ── 잎 마디 ──
	// 함수를 그대로 담는다. 마디마다 클래스를 만들지 않아도 되도록.
	class Condition : public Node
	{
	public:
		typedef bool (*Test)(Blackboard&);
		explicit Condition(Test test) : m_Test(test) {}
		Status Tick(Blackboard& board) override;

	private:
		Test m_Test;
	};

	class Action : public Node
	{
	public:
		typedef Status (*Run)(Blackboard&);
		explicit Action(Run run) : m_Run(run) {}
		Status Tick(Blackboard& board) override;

	private:
		Run m_Run;
	};

	// 만들기 편하도록
	NodePtr MakeSelector();
	NodePtr MakeSequence();
	NodePtr MakeCondition(Condition::Test test);
	NodePtr MakeAction(Action::Run run);
}
