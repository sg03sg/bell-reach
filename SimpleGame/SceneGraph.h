#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Actor.h"

//
// 장면 그래프.
//
// 화면에 놓인 액터들을 나무 구조로 들고 있으면서 한 프레임의 순서를 맡는다.
//
//   갱신 : 뿌리에서 잎으로 내려가며 자리를 누적하고(부모 → 자식) OnUpdate를 부른다.
//   그리기: 보이는 액터를 모아 층과 깊이로 정렬한 뒤 OnDraw를 부른다.
//
// 그리기를 순회 순서가 아니라 정렬 순서로 하는 것이 중요하다. 2.5D에서는
// 화면 아래쪽에 선 것이 앞에 와야 하므로, 나무 구조의 순서가 아니라 발밑
// 좌표가 앞뒤를 정한다.
//
// 액터는 갱신 도중에도 마음대로 새 액터를 놓고 자신을 지울 수 있다. 실제로
// 트리가 바뀌는 것은 순회가 다 끝난 뒤라서, 도는 도중에 목록이 흔들리지 않는다.
//
class SceneGraph
{
public:
	SceneGraph();

	Actor& Root() { return *m_Root; }
	const Actor& Root() const { return *m_Root; }

	// 장면에 액터를 놓는다. parent가 없으면 뿌리에 붙는다.
	// 갱신 도중에 불러도 안전하다.
	Actor* Spawn(std::unique_ptr<Actor> actor, Actor* parent = NULL);

	void Update(const ActorContext& context);
	void Draw(const DrawContext& context, unsigned int layerMask) const;

	static unsigned int MaskOf(Actor::Layer layer) { return 1u << static_cast<int>(layer); }
	static unsigned int AllLayers() { return 0xFFFFFFFFu; }

	size_t ActorCount() const { return m_Count; }

	// 트리 전체를 훑는다. visit(Actor&) 를 받는다.
	template <typename Visit>
	void ForEach(Visit visit)
	{
		Walk(m_Root.get(), visit);
	}

	template <typename Visit>
	void ForEach(Visit visit) const
	{
		Walk(m_Root.get(), visit);
	}

private:
	template <typename Visit>
	static void Walk(Actor* actor, Visit& visit)
	{
		visit(*actor);
		for (const std::unique_ptr<Actor>& child : actor->Children())
		{
			Walk(child.get(), visit);
		}
	}

	void UpdateActor(Actor* actor, const ActorContext& context,
	                 float parentX, float parentY, float parentRotation, float parentScale);
	void Collect(Actor* actor, unsigned int layerMask) const;
	void FlushPending();

	std::unique_ptr<Actor> m_Root;

	// 갱신 도중에 놓인 액터들. 순회가 끝난 뒤에 트리로 들어간다.
	std::vector<std::pair<std::unique_ptr<Actor>, Actor*>> m_Pending;

	// 프레임마다 다시 쓰는 그리기 목록. 매번 새로 할당하지 않는다.
	mutable std::vector<Actor*> m_DrawList;
	size_t m_Count = 1;
};
