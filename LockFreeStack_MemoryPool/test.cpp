#include <stdio.h>
#include <process.h>
#include <conio.h>
#include "LockFreeStack.h"

#pragma comment(lib, "winmm")

union CUSTOM_LPVOID
{
	LPVOID args;
	int64_t i64Arg;
	int32_t i32Args[2];
	int16_t i16Args[4];
	int8_t i8Args[8];
};

unsigned WINAPI TestLockFreeStack(CUSTOM_LPVOID);

bool running = true;
Stack<int*> stackIntPtr;
int main()
{
	int key;
	HANDLE hThread[4];
	for (int64_t i = 0; i < 4; ++i)
	{
		hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, (_beginthreadex_proc_type)TestLockFreeStack, (LPVOID)i, 0, nullptr);
	}

	for (int i = 0; i < 4000; ++i)
	{
		int* ptrI32 = new int;
		*ptrI32 = 0x05555555;
		stackIntPtr.Push(ptrI32);
	}

	while (running)
	{
		if (_kbhit())
		{
			key = _getch();
			if (key == 'q' || key == 'Q')
			{
				running = false;
				printf("종료 요청\n");
			}
			else if (key == 'p' || key == 'P')
			{
				printf("StackCnt: %lld\n", stackIntPtr.GetSize());
			}
		}

		Sleep(0);
	}

	WaitForMultipleObjects(4, hThread, true, INFINITE);
	printf("끝\n");
	for (int i = 0; i < 4; ++i)
	{
		CloseHandle(hThread[i]);
	}

	int* ptrI32;
	for (int i = 0; i < 4000; ++i)
	{
		stackIntPtr.Pop(ptrI32);
		delete ptrI32;
	}

	return 0;
}

int gValue = 0;
int gPopIndex = 0;
Stack<int> stack;
LockFreeHistory* history[4];
unsigned WINAPI TestLockFreeStack(CUSTOM_LPVOID args)
{
	history[args.i64Arg] = new LockFreeHistory[50000];
	memset(history[args.i64Arg], 0, sizeof(LockFreeHistory) * 50000);
	//srand(time(nullptr));
	int pop;
	int index = 0;
	int localValue;
	int popIndex;

	int* ptrArr[1000] = { 0, };

	while (running)
	{
		for (int i = 0; i < 1000; ++i)
		{
			stackIntPtr.Pop(ptrArr[i]);
			if ((*ptrArr[i]) != 0x05555555)
			{
				*((char*)0x8123) = 0;
			}
		}

		for (int i = 0; i < 1000; ++i)
		{
			(*ptrArr[i]) += i;
			if (*ptrArr[i] != 0x05555555 + i)
			{
				*((char*)0x8123) = 0;
			}
		}

		for (int i = 0; i < 1000; ++i)
		{
			*ptrArr[i] = 0x05555555;
			if ((*ptrArr[i]) != 0x05555555)
			{
				*((char*)0x8123) = 0;
			}
			stackIntPtr.Push(ptrArr[i]);
		}
		
	}
	
	return 0;
}
