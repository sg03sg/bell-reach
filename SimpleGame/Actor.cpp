#include "stdafx.h"
#include "Actor.h"

#include <algorithm>
#include <cmath>

Actor::Actor(const char* name)
	: m_Name(name != NULL ? name : "Actor")
{
}

Actor::~Actor()
{
}

Actor* Actor::AddChild(std::unique_ptr<Actor> child)
{
	if (!child)
	{
		return NULL;
	}

	Actor* raw = child.get();
	raw->m_Parent = this;
	m_Children.push_back(std::move(child));
	return raw;
}

void Actor::ApplyParentTransform(float parentX, float parentY, float parentRotation, float parentScale)
{
	// 부모가 돌아 있으면 자식의 자리도 함께 돌아간다.
	const float c = std::cos(parentRotation);
	const float s = std::sin(parentRotation);
	const float scaledX = m_X * parentScale;
	const float scaledY = m_Y * parentScale;

	m_WorldX = parentX + scaledX * c - scaledY * s;
	m_WorldY = parentY + scaledX * s + scaledY * c;
	m_WorldRotation = parentRotation + m_Rotation;
	m_WorldScale = parentScale * m_Scale;
}

void Actor::PruneDestroyed()
{
	// 지워진 액터는 자식까지 통째로 빠진다. unique_ptr이므로 메모리도 함께 정리된다.
	m_Children.erase(std::remove_if(m_Children.begin(), m_Children.end(),
	                                [](const std::unique_ptr<Actor>& child)
	                                {
	                                	return child->IsDestroyed();
	                                }),
	                 m_Children.end());

	for (std::unique_ptr<Actor>& child : m_Children)
	{
		child->PruneDestroyed();
	}
}

DrawTag Actor::Tag() const
{
	if (m_Tag < 0)
	{
		m_Tag = static_cast<int>(DrawTagFromActorName(m_Name.c_str()));
	}
	return static_cast<DrawTag>(m_Tag);
}
