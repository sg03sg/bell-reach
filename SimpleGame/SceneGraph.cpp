#include "stdafx.h"
#include "SceneGraph.h"

#include "Renderer.h"

#include <algorithm>

SceneGraph::SceneGraph()
	: m_Root(new Actor("Scene"))
{
	m_Root->SetLayer(Actor::Layer::Ground);
	m_Root->SetVisible(false);   // 뿌리는 자리만 잡아 줄 뿐 그려지지 않는다
}

Actor* SceneGraph::Spawn(std::unique_ptr<Actor> actor, Actor* parent)
{
	if (!actor)
	{
		return NULL;
	}

	Actor* raw = actor.get();
	m_Pending.push_back(std::make_pair(std::move(actor), parent != NULL ? parent : m_Root.get()));
	return raw;
}

void SceneGraph::FlushPending()
{
	// 트리에 들이면서 자리를 바로 잡아 준다. 놓인 프레임에도 제자리에 그려지도록.
	for (size_t i = 0; i < m_Pending.size(); ++i)
	{
		Actor* parent = m_Pending[i].second;
		Actor* child = parent->AddChild(std::move(m_Pending[i].first));
		if (child != NULL)
		{
			child->ApplyParentTransform(parent->WorldX(), parent->WorldY(),
			                            parent->WorldRotation(), parent->WorldScale());
		}
	}
	m_Pending.clear();
}

void SceneGraph::UpdateActor(Actor* actor, const ActorContext& context,
                             float parentX, float parentY, float parentRotation, float parentScale)
{
	actor->ApplyParentTransform(parentX, parentY, parentRotation, parentScale);
	++m_Count;

	if (actor->IsActive() && !actor->IsDestroyed())
	{
		actor->OnUpdate(context);
	}

	// 자식은 갱신된 부모의 자리를 기준으로 한다.
	// 부모가 OnUpdate에서 움직였다면 자식도 같은 프레임에 따라 움직인다.
	const std::vector<std::unique_ptr<Actor>>& children = actor->Children();
	for (size_t i = 0; i < children.size(); ++i)
	{
		UpdateActor(children[i].get(), context,
		            actor->WorldX(), actor->WorldY(), actor->WorldRotation(), actor->WorldScale());
	}
}

void SceneGraph::Update(const ActorContext& context)
{
	// 지난 프레임에 놓인 액터를 먼저 들인다.
	FlushPending();

	m_Count = 0;
	UpdateActor(m_Root.get(), context, 0.0f, 0.0f, 0.0f, 1.0f);

	// 도는 도중에 놓인 액터들을 들이고, 지워진 액터들을 치운다.
	FlushPending();
	m_Root->PruneDestroyed();
}

void SceneGraph::Collect(Actor* actor, unsigned int layerMask) const
{
	if (actor->IsDestroyed())
	{
		return;
	}

	if (actor->IsVisible() && (layerMask & MaskOf(actor->GetLayer())) != 0)
	{
		m_DrawList.push_back(actor);
	}

	// 부모가 보이지 않아도 자식은 보일 수 있다. 자리를 잡아 주기만 하는
	// 액터(뿌리처럼)를 두기 위해서다.
	for (const std::unique_ptr<Actor>& child : actor->Children())
	{
		Collect(child.get(), layerMask);
	}
}

void SceneGraph::Draw(const DrawContext& context, unsigned int layerMask) const
{
	m_DrawList.clear();
	Collect(m_Root.get(), layerMask);

	// 층이 먼저, 같은 층 안에서는 발밑이 위에 있는 것부터.
	// 같은 깊이면 놓인 순서를 지켜야 깜빡이지 않으므로 stable_sort를 쓴다.
	std::stable_sort(m_DrawList.begin(), m_DrawList.end(),
	                 [](const Actor* a, const Actor* b)
	                 {
	                 	if (a->GetLayer() != b->GetLayer())
	                 	{
	                 		return a->GetLayer() < b->GetLayer();
	                 	}
	                 	return a->SortDepth() < b->SortDepth();
	                 });

	for (size_t i = 0; i < m_DrawList.size(); ++i)
	{
		// 액터마다 꼬리표를 바꿔 두면, 성능 로그가 "무엇이 몇 개를
		// 그렸는지"를 종류별로 나눠 보여 준다. 액터 쪽은 손댈 것이 없다.
		context.renderer->SetTag(m_DrawList[i]->Tag());
		m_DrawList[i]->OnDraw(context);
	}
	context.renderer->SetTag(DrawTag::Other);
}
