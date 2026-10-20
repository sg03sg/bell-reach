#include "stdafx.h"
#include "BehaviorTree.h"

namespace BT
{
	void Composite::Reset()
	{
		m_Current = 0;
		for (size_t i = 0; i < m_Children.size(); ++i)
		{
			m_Children[i]->Reset();
		}
	}

	//
	// 왼쪽부터 시도해 처음 성공하거나 진행 중인 것을 고른다.
	// 전부 실패해야 비로소 실패다.
	//
	// 진행 중인 자식을 기억해 두었다가 다음 프레임에 이어서 하되,
	// 그 앞의 자식들은 매번 다시 본다. 그래야 걷는 도중에 플레이어가
	// 다가오면 하던 일을 멈추고 위쪽 가지로 넘어갈 수 있다.
	//
	Status Selector::Tick(Blackboard& board)
	{
		for (size_t i = 0; i < m_Children.size(); ++i)
		{
			const Status status = m_Children[i]->Tick(board);
			if (status == Status::Failure)
			{
				continue;
			}

			// 더 급한 가지가 끼어들었으면, 하던 가지를 정리한다.
			if (status == Status::Running && m_Current != i)
			{
				if (m_Current < m_Children.size())
				{
					m_Children[m_Current]->Reset();
				}
				m_Current = i;
			}
			return status;
		}

		m_Current = 0;
		return Status::Failure;
	}

	//
	// 왼쪽부터 차례로 해낸다. 하나라도 실패하면 거기서 멈추고 실패다.
	//
	// 진행 중이던 자리를 기억하지 않고 매 프레임 처음부터 다시 훑는다.
	// 앞에 놓인 조건 마디가 계속 다시 검사되어야, 걷는 도중에 플레이어가
	// 다가왔을 때 하던 일을 멈추고 위쪽 가지로 넘어갈 수 있기 때문이다.
	// (조건 → 행동 순서로만 쓰므로 조건을 다시 보는 비용은 없다시피 하다)
	//
	Status Sequence::Tick(Blackboard& board)
	{
		for (size_t i = 0; i < m_Children.size(); ++i)
		{
			const Status status = m_Children[i]->Tick(board);
			if (status != Status::Success)
			{
				return status;
			}
		}
		return Status::Success;
	}

	Status Condition::Tick(Blackboard& board)
	{
		return m_Test(board) ? Status::Success : Status::Failure;
	}

	Status Action::Tick(Blackboard& board)
	{
		return m_Run(board);
	}

	NodePtr MakeSelector()  { return NodePtr(new Selector()); }
	NodePtr MakeSequence()  { return NodePtr(new Sequence()); }
	NodePtr MakeCondition(Condition::Test test) { return NodePtr(new Condition(test)); }
	NodePtr MakeAction(Action::Run run)         { return NodePtr(new Action(run)); }
}
