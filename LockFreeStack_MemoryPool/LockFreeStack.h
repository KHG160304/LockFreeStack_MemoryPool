#pragma once
#ifndef __LOCK_FREE_STACK_H__
#define	__LOCK_FREE_STACK_H__
#include "LockFreeMemoryPool.h"
#include <stdint.h>

enum eLockFreeFuncType
{
	PUSH, POP
};

struct LockFreeHistory {
	eLockFreeFuncType funType;
	DWORD threadID;
	int value;
	int nextValue;
	int ccurrentNextValue;
	__int64* top;
	__int64* topNext;
	__int64* topCurrentNext;
	int gPopIndex;
};

template <typename T>
class Stack
{
private:
	struct Node
	{
		Node* next;
		T object;
	};

public:
	Stack() 
		: mTop(nullptr)
		, mSize(0)
	{
	
	}

	size_t GetSize()
	{
		return mSize;
	}

	void Push(T& param)
	{
		Stack::Node* ptrNode = mAllocator.Alloc();
		ptrNode->object = param;

		SequencedPtr<Stack::Node> oldTop(mTop);
		SequencedPtr<Stack::Node> newTop(ptrNode, oldTop.GetSequence());

		uint64_t* ptrOldTop = (uint64_t*)&oldTop;
		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		for (;;)
		{
			ptrNode->next = oldTop.GetPtr();
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				break;
			}

			oldTop = mTop;
			newTop.SetSequenece(oldTop.GetSequence());
		}
		InterlockedIncrement(&mSize);
	}

	void PushDebug(T& param, LockFreeHistory* history)
	{
		Stack::Node* ptrNode = mAllocator.Alloc();
		ptrNode->object = param;

		SequencedPtr<Stack::Node> oldTop(mTop);
		SequencedPtr<Stack::Node> newTop(ptrNode, oldTop.GetSequence());

		uint64_t* ptrOldTop = (uint64_t*)&oldTop;
		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		for (;;)
		{
			ptrNode->next = oldTop.GetPtr();
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				break;
			}

			oldTop = mTop;
			newTop.SetSequenece(oldTop.GetSequence());
		}
		InterlockedIncrement(&mSize);

		history->funType = PUSH;
		history->threadID = GetCurrentThreadId();
		history->top = (__int64*)newTop.GetPtr();
		history->topNext = (__int64*)oldTop.GetPtr();
		history->value = param;
		if (history->topNext != nullptr)
		{
			history->nextValue = oldTop->object;
		}
		else
		{
			history->nextValue = -1;
		}
		
	}

	bool Pop(T& param)
	{
		SequencedPtr<Stack::Node> oldTop(mTop);
		typename SequencedPtr<T>::sequenced_ptr_t newTop;

		uint64_t* ptrOldTop = (uint64_t*)&oldTop;
		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		for (;;)
		{
			if (oldTop.GetPtr() == nullptr)
			{
				return false;
			}

			((SequencedPtr<Stack::Node>*)&newTop)->Set(oldTop->next, oldTop.GetNextSequence());
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				break;
			}
			oldTop = mTop;
		}
		InterlockedDecrement(&mSize);

		param = oldTop->object;
		mAllocator.Free(oldTop.GetPtr());
	}

	bool PopDebug(T& param, LockFreeHistory* history, int gPopIndex)
	{
		SequencedPtr<Stack::Node> oldTop(mTop);
		typename SequencedPtr<T>::sequenced_ptr_t newTop;

		Stack::Node* _currentTopNext;
		int localCurrentNextValue = -1;

		uint64_t* ptrOldTop = (uint64_t*)&oldTop;
		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		for (;;)
		{
			if (oldTop.GetPtr() == nullptr)
			{
				return false;
			}
			_currentTopNext = mTop->next;
			if (_currentTopNext != nullptr)
			{
				localCurrentNextValue = _currentTopNext->object;
			}
			((SequencedPtr<Stack::Node>*)&newTop)->Set(oldTop->next, oldTop.GetNextSequence());
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				break;
			}
			oldTop = mTop;
		}
		InterlockedDecrement(&mSize);
		param = oldTop->object;

		history->funType = POP;
		history->threadID = GetCurrentThreadId();
		history->top = (__int64*)oldTop.GetPtr();
		history->topNext = (__int64*)((SequencedPtr<Stack::Node>*)&newTop)->GetPtr();
		history->value = param;
		if (history->topNext != nullptr)
		{
			history->nextValue = (*((SequencedPtr<Stack::Node>*) & newTop))->object;
		}
		else
		{
			history->nextValue = -1;
		}
		
		history->ccurrentNextValue = localCurrentNextValue;
		history->topCurrentNext = (__int64*)_currentTopNext;
		history->gPopIndex = gPopIndex;

		mAllocator.Free(oldTop.GetPtr());
	}

private:
	SequencedPtr<Stack::Node> mTop;
	MemoryPool<Stack::Node> mAllocator;
	size_t mSize;
};

#endif // !__LOCK_FREE_STACK_H__


