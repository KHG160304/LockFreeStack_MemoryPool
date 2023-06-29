//////////////////////////////////////////////////////////////////////
//														������
//
//	�޸� Ǯ
//	���α׷����� ����ϱ� ���ؼ� �ѹ� ������ ��ü��(�޸�)��
//  �����ϱ� ���ؼ�, ��ü���� �������ִ� �������� List�� �������� ������, Stack��
//	�ܼ��� ���۹���� ���ϰ� �ִ� �ڷᱸ��
// 
//	����
//	1. ��ü Ÿ�Ը���. �޸� Ǯ�� �����Ͽ� ���
//	2. �ش� ��ü Ÿ���� �ʿ��� ��, �޸�Ǯ����, Alloc�� ȣ���Ͽ�, ��ü�� �ּҸ� �� ���.
//  3. ��ü����� ���ϰ� �� �Ŀ�, �ݵ�� �޸� Ǯ���� Free�Լ��� ȣ���Ͽ�, ��ü�� ��ȯ
//	
//	�� ���ǻ���
//	1. �޸�Ǯ�� ��û�Ͽ���, ���� ��ü�� �ƴ� 
//	   ����ڰ� ���Ƿ� ������ ��ü�� Free �Լ��� ȣ���Ͽ� ��ȯ�ϸ� �ȵȴ�.
//	   (�˻��Ͽ�, ������ �߻���Ų��.)
//	2. ����ڴ� �޸�Ǯ�� ��û�Ͽ�, ���� ��� ��ü�� �޸� Ǯ�� ��ȯ�ϱ� ������
//	   �޸�Ǯ�� �����ؼ��� �ȵȴ�.
//  3. ����ڰ� �޸�Ǯ�� ��û�Ͽ� ������� ��ü��
//	   �޸�Ǯ�� ���� ��󿡼� �����ȴ�.
//	4. �߸��� ������� ���ؼ�, ������ �߻��� �� �ֽ��ϴ�.
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
	{}

	MemoryPool(int capacity, bool isPlacementNew = false)
		: mTop(nullptr)
		, mLargeNode(nullptr)
		, mLargeNodeSize(capacity * sizeof(Node))
		, mAllocCnt(0)
		, mFreeCnt(capacity)
		, mIsPlacementNew(isPlacementNew)
	{
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
	//��ü Ǯ���� ����ڿ��� �Ҵ����� ��ü�� ������ �����Ѵ�.
	//
	//Paramters: ����
	//Retrun: (int) ��ü �Ҵ� ����
	//////////////////////////////////////////////////////////////
	int GetAllocCnt()
	{
		return mAllocCnt;
	}

	//////////////////////////////////////////////////////////////
	//��ü Ǯ���� ���� �����ϰ� �ִ� �ν��Ͻ��� ������ �����Ѵ�.
	//
	//Paramters: ����
	//Retrun: (int) Ǯ���� �����ϰ� �ִ� �ν��Ͻ� ����
	//////////////////////////////////////////////////////////////
	int GetFreeCnt()
	{
		return mFreeCnt;
	}

	//////////////////////////////////////////////////////
	//��ü�� �Ҵ��� ��û�Ҷ� ȣ���ϴ� �Լ�
	//
	//Paramters: ����
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
					//���� �޸� Ǯ �ν��Ͻ��� �ּҰ��� �Ҵ����� ��ü�� ���δ� ����� �ּҰ���
					//���� xor �����Ͽ�, �޸� ħ�� Ȯ�ο� ��Ű ���� �����Ѵ�.
					securityCookie = (size_t)this xor (size_t)popNode;
					popNode->next = (Node*)securityCookie;
					popNode->overflowCookie = securityCookie;
#endif				// MEMORY_POOL_DEBUG
					return new ((char*)popNode + sizeof(Node*)) T;
				}
#ifdef			MEMORY_POOL_DEBUG
				//���� �޸� Ǯ �ν��Ͻ��� �ּҰ��� 
				//�Ҵ����� ��ü�� ���δ� ����� �ּҰ���
				//���� xor �����Ͽ�, �޸� ħ�� Ȯ�ο� ��Ű ���� �����Ѵ�.
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
		//���� �޸� Ǯ �ν��Ͻ��� �ּҰ��� �Ҵ����� ��ü�� ���δ� ����� �ּҰ���
		//���� xor �����Ͽ�, �޸� ħ�� Ȯ�ο� ��Ű ���� �����Ѵ�.
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
	//�Ҵ���� ��ü�� �����Ҷ� ȣ���ϴ� �Լ�
	//
	//Paramters: (T*) object
	//Retrun: ����
	//////////////////////////////////////////////////////
	void Free(T* ptrObject)
	{
		Node* node = (Node*)((char*)ptrObject - sizeof(Node*));
#ifdef MEMORY_POOL_DEBUG
		/*
			�޸� ħ�� ���(�����÷ο�, ����÷ο�),
			�ߺ��Ҵ����� ���
		*/
		if (node->overflowCookie != (size_t)node->next)
		{
			*((char*)0x8123) = 0;
			return;
		}
		/*
			�ٸ� Ǯ���� ������ ��ü�� �Ҵ������� ��û�ߴ��� �ɷ����� ���ؼ�
			Cookie�� �Ľ��� ����, ���� Ǯ�� �ּҰ��� �������� üũ
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
