//////////////////////////////////////////////////////////////////////
//														강현구
//
//	메모리 풀
//	프로그램에서 사용하기 위해서 한번 생성한 객체들(메모리)을
//  재사용하기 위해서, 객체들을 관리해주는 목적으로 List의 가변길이 구조와, Stack의
//	단순한 동작방식을 취하고 있는 자료구조
// 
//	사용법
//	1. 객체 타입마다. 메모리 풀을 생성하여 사용
//	2. 해당 객체 타입이 필요할 시, 메모리풀에서, Alloc을 호출하여, 객체의 주소를 얻어서 사용.
//  3. 객체사용을 다하고 난 후에, 반드시 메모리 풀에서 Free함수를 호출하여, 객체를 반환
//	
//	※ 주의사항
//	1. 메모리풀에 요청하여서, 얻은 객체가 아닌 
//	   사용자가 임의로 생성한 객체를 Free 함수를 호출하여 반환하면 안된다.
//	   (검사하여, 에러를 발생시킨다.)
//	2. 사용자는 메모리풀에 요청하여, 얻은 모든 객체를 메모리 풀에 반환하기 이전에
//	   메모리풀을 삭제해서는 안된다.
//  3. 사용자가 메모리풀에 요청하여 사용중인 객체는
//	   메모리풀의 관리 대상에서 삭제된다.
//	4. 잘못된 사용으로 인해서, 에러가 발생할 수 있습니다.
//  
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __LOCK_FREE_MEMORY_POOL_H__
#define __LOCK_FREE_MEMORY_POOL_H__

#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <new>

template <typename T>
class SequencedPtr
{

public:
	typedef uint64_t sequenced_ptr_t;
	typedef	uint16_t sequence_t;
private:
	static constexpr int SEQUENCE_INDEX = 3;
	static constexpr sequenced_ptr_t PTR_MASK = 0x0000ffffffffffffUL;
	static constexpr sequence_t MAX_SEQUENCE = 0xffffU;

	union SequencedPtrType
	{
		sequenced_ptr_t ptr;
		sequence_t sequence[4];
	};

public:
	explicit SequencedPtr(T* ptr, sequence_t sequence = 0)
		: mSequencedPtr((sequenced_ptr_t)ptr)
	{
		((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX] = sequence;
	};

	SequencedPtr(const SequencedPtr& copy)
		: mSequencedPtr(copy.mSequencedPtr)
	{

	};

	T& operator*() const
	{
		return *((T*)(PTR_MASK & mSequencedPtr));
	}

	T* operator->() const
	{
		return (T*)(PTR_MASK & mSequencedPtr);
	}

	T* GetPtr() const
	{
		return (T*)(PTR_MASK & mSequencedPtr);
	}

	void Set(T* ptr, sequence_t sequence)
	{
		mSequencedPtr = sequenced_ptr_t(ptr);
		((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX] = sequence;
	}

	void SetPtr(T* ptr)
	{
		sequence_t sequence = ((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX];
		mSequencedPtr = sequenced_ptr_t(ptr);
		((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX] = sequence;
	}

	sequence_t GetSequence()
	{
		return ((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX];
	}

	void SetSequenece(sequence_t sequence)
	{
		((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX] = sequence;
	}

	sequence_t GetNextSequence()
	{
		return (((SequencedPtrType*)&mSequencedPtr)->sequence[SEQUENCE_INDEX] + 1U) & MAX_SEQUENCE;
	}

private:
	sequenced_ptr_t mSequencedPtr;
};

template <typename T>
class MemoryPool
{
public:
	MemoryPool(bool isPlacementNew = false)
		: mTop(nullptr)
		, mLargeNode(nullptr)
		, mLargeNodeSize(0)
		, mAllocCnt(0)
		, mFreeCnt(0)
		, mIsPlacementNew(isPlacementNew)
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		uint64_t maximumApplicationAddress = (uint64_t)sysInfo.lpMaximumApplicationAddress;
		if (maximumApplicationAddress & 0xFFFF000000000000UL)
		{
			printf("64bit Address Space Range is Exntended\n");
			*((char*)0x8123) = 0;
		}
	}

	MemoryPool(int capacity, bool isPlacementNew = false)
		: mTop(nullptr)
		, mLargeNode(nullptr)
		, mLargeNodeSize(capacity * sizeof(Node))
		, mAllocCnt(0)
		, mFreeCnt(capacity)
		, mIsPlacementNew(isPlacementNew)
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		uint64_t maximumApplicationAddress = (uint64_t)sysInfo.lpMaximumApplicationAddress;
		if (maximumApplicationAddress & 0xFFFF000000000000UL)
		{
			printf("64bit Address Space Range is Exntended\n");
			*((char*)0x8123) = 0;
		}

		if (isPlacementNew)
		{
			mLargeNode = (Node*)malloc(sizeof(Node) * capacity);
		}
		else
		{
			mLargeNode = new Node[capacity];
		}

		if (mLargeNode == nullptr)
		{
			*((char*)0x8123) = 0;
			return;
		}

		mTop.SetPtr(mLargeNode);

		int cnt = capacity - 1;
		for (int i = 0; i < cnt; ++i)
		{
			mLargeNode[i].next = mLargeNode + i + 1;
		}
		mLargeNode[cnt].next = nullptr;
	}

	~MemoryPool()
	{
		Node* ptrTopNode = mTop.GetPtr();
		Node* ptrDeleteNode;
		size_t largeNodeOffset;

		if (mIsPlacementNew)
		{
			if (mLargeNode == nullptr)
			{
				while (ptrTopNode != nullptr)
				{
					ptrDeleteNode = ptrTopNode;
					ptrTopNode = ptrTopNode->next;
					free(ptrDeleteNode);
				}
				return;
			}

			while (ptrTopNode != nullptr)
			{
				largeNodeOffset = ((size_t)ptrTopNode - (size_t)mLargeNode);
				if (largeNodeOffset >= 0 && largeNodeOffset < mLargeNodeSize)
				{
					ptrTopNode = ptrTopNode->next;
					continue;
				}

				ptrDeleteNode = ptrTopNode;
				ptrTopNode = ptrTopNode->next;
				free(ptrDeleteNode);
			}
			free(mLargeNode);
		}
		else
		{
			if (mLargeNode == nullptr)
			{
				while (ptrTopNode != nullptr)
				{
					ptrDeleteNode = ptrTopNode;
					ptrTopNode = ptrTopNode->next;
					delete ptrDeleteNode;
				}
				return;
			}

			while (ptrTopNode != nullptr)
			{
				largeNodeOffset = ((size_t)ptrTopNode - (size_t)mLargeNode);
				if (largeNodeOffset > -1 && largeNodeOffset < mLargeNodeSize)
				{
					ptrTopNode = ptrTopNode->next;
					continue;
				}

				ptrDeleteNode = ptrTopNode;
				ptrTopNode = ptrTopNode->next;
				delete ptrDeleteNode;
			}
			delete[] mLargeNode;
		}
	}

	//////////////////////////////////////////////////////////////
	//객체 풀에서 사용자에게 할당해준 객체의 갯수를 리턴한다.
	//
	//Paramters: 없음
	//Retrun: (int) 객체 할당 갯수
	//////////////////////////////////////////////////////////////
	int GetAllocCnt()
	{
		return mAllocCnt;
	}

	//////////////////////////////////////////////////////////////
	//객체 풀에서 현재 관리하고 있는 인스턴스의 갯수를 리턴한다.
	//
	//Paramters: 없음
	//Retrun: (int) 풀에서 관리하고 있는 인스턴스 갯수
	//////////////////////////////////////////////////////////////
	int GetFreeCnt()
	{
		return mFreeCnt;
	}

	//////////////////////////////////////////////////////
	//객체를 할당을 요청할때 호출하는 함수
	//
	//Paramters: 없음
	//Retrun: T*
	//////////////////////////////////////////////////////
	T* Alloc()
	{
		Node* popNode;
		SequencedPtr<Node> oldTop = mTop;
		typename SequencedPtr<T>::sequenced_ptr_t newTop;

		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		uint64_t* ptrOldTop = (uint64_t*)&oldTop;
#ifdef MEMORY_POOL_DEBUG
		size_t securityCookie;
#endif // MEMORY_POOL_DEBUG

		for (;;)
		{
			if (oldTop.GetPtr() == nullptr)
			{
				InterlockedIncrement((ULONG*)&mAllocCnt);
				if (mIsPlacementNew)
				{
					popNode = (Node*)malloc(sizeof(Node));
					if (popNode == nullptr)
					{
						*((char*)0x8123) = 0;
						return nullptr;
					}
#ifdef				MEMORY_POOL_DEBUG
					//현재 메모리 풀 인스턴스의 주소값과 할당해줄 객체를 감싸는 노드의 주소값을
					//서로 xor 연산하여, 메모리 침범 확인용 쿠키 값을 생성한다.
					securityCookie = (size_t)this xor (size_t)popNode;
					popNode->next = (Node*)securityCookie;
					popNode->overflowCookie = securityCookie;
#endif				// MEMORY_POOL_DEBUG
					return new ((char*)popNode + sizeof(Node*)) T;
				}
#ifdef			MEMORY_POOL_DEBUG
				//현재 메모리 풀 인스턴스의 주소값과 
				//할당해줄 객체를 감싸는 노드의 주소값을
				//서로 xor 연산하여, 메모리 침범 확인용 쿠키 값을 생성한다.
				popNode = new Node;
				securityCookie = (size_t)this xor (size_t)popNode;
				popNode->next = (Node*)securityCookie;
				popNode->overflowCookie = securityCookie;
				return (T*)((char*)popNode + sizeof(Node*));
#else
				return (T*)(((char*)new Node) + sizeof(Node*));
#endif			// MEMORY_POOL_DEBUG
			}

			((SequencedPtr<Node>*) & newTop)->Set(oldTop->next, oldTop.GetNextSequence());
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				popNode = oldTop.GetPtr();
				break;
			}

			oldTop = mTop;
		}

		InterlockedIncrement((ULONG*)&mAllocCnt);
		InterlockedDecrement((ULONG*)&mFreeCnt);

#ifdef MEMORY_POOL_DEBUG
		//현재 메모리 풀 인스턴스의 주소값과 할당해줄 객체를 감싸는 노드의 주소값을
		//서로 xor 연산하여, 메모리 침범 확인용 쿠키 값을 생성한다.
		securityCookie = (size_t)this xor (size_t)popNode;
		popNode->next = (Node*)securityCookie;
		popNode->overflowCookie = securityCookie;
#endif // MEMORY_POOL_DEBUG

		if (mIsPlacementNew)
		{
			return new ((char*)popNode + sizeof(Node*)) T;
		}
		return (T*)((char*)popNode + sizeof(Node*));
	}

	//////////////////////////////////////////////////////
	//할당받은 객체를 해제할때 호출하는 함수
	//
	//Paramters: (T*) object
	//Retrun: 없음
	//////////////////////////////////////////////////////
	void Free(T* ptrObject)
	{
		Node* node = (Node*)((char*)ptrObject - sizeof(Node*));
#ifdef MEMORY_POOL_DEBUG
		/*
			메모리 침범 방어(오버플로우, 언더플로우),
			중복할당해제 방어
		*/
		if (node->overflowCookie != (size_t)node->next)
		{
			*((char*)0x8123) = 0;
			return;
		}
		/*
			다른 풀에서 생성된 객체로 할당해제를 요청했는지 걸러내기 위해서
			Cookie를 파싱한 값이, 현재 풀의 주소값과 동일한지 체크
		*/
		if ((MemoryPool<T>*)(node->overflowCookie xor (size_t)node) != this)
		{
			*((char*)0x8123) = 0;
			return;
		}
#endif // MEMORY_POOL_DEBUG

		SequencedPtr<Node> oldTop(mTop);
		SequencedPtr<Node> newTop(node, oldTop.GetSequence());

		uint64_t* ptrNewTop = (uint64_t*)&newTop;
		uint64_t* ptrOldTop = (uint64_t*)&oldTop;

		for (;;)
		{
			node->next = oldTop.GetPtr();
			if (InterlockedCompareExchange((uint64_t*)&mTop, *ptrNewTop, *ptrOldTop) == *ptrOldTop)
			{
				break;
			}

			oldTop = mTop;
			newTop.SetSequenece(oldTop.GetSequence());
		}

		InterlockedDecrement((ULONG*)&mAllocCnt);
		InterlockedIncrement((ULONG*)&mFreeCnt);

		if (mIsPlacementNew)
		{
			ptrObject->~T();
		}
	}

private:
	struct Node
	{
		Node* next;
		T object;
#ifdef MEMORY_POOL_DEBUG
		size_t overflowCookie;
#endif
	};

	SequencedPtr<Node> mTop;
	Node* mLargeNode;
	size_t mLargeNodeSize;
	int mAllocCnt;
	int mFreeCnt;
	int mIsPlacementNew;
};

#endif // !__LOCK_FREE_MEMORY_POOL_H__
