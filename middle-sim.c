#include <stdio.h>
#include <windows.h>
#include <winuser.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define DINFO  eprintf
#define DERROR eprintf

#define CHECK(expr, ...) do {                 \
		if (!(expr)) {                        \
			DERROR(#expr " check failed\n");  \
			return __VA_ARGS__;               \
		}                                     \
	} while (0)

static HINSTANCE g_hInst = NULL;
static HHOOK g_hook = NULL;
static UINT_PTR g_timer = 0;

int g_down[2] = {};
int g_both = 0;
int g_timeout = 0;

static void TimerFunc(HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4);

static void StartTimer()
{
	DINFO("StartTimer\n");
	
	g_timer = SetTimer(NULL, 0, 50, TimerFunc);
	if (g_timer == 0)
		DERROR("SetTimer failed: %x\n", GetLastError());
}

static void StopTimer()
{
	DINFO("StopTimer\n");

	if (!KillTimer(NULL, g_timer))
		DERROR("KillTimer failed: %x\n", GetLastError());
}

static void Inject(int btn, int down)
{
	int flags;
	
	if (btn == 0)
		flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
	else if (btn == 1)
		flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
	else if (btn == 2)
		flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;			
	else {
		DERROR("Unknown button: %d\n", btn);
		return;
	}
	
	INPUT input = {};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;

	DINFO("+Inject %d %d\n", btn, down);	
	
	if (SendInput(1, &input, sizeof(INPUT)) != 1)
		DERROR("SendInput failed: %d\n", GetLastError());
		
	DINFO("-Inject %d %d\n", btn, down);
}

static void TimerFunc(HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4)
{
	DINFO("+Timer\n");

	StopTimer();

	if (g_down[0])
	{
		Inject(0, 1);
		CHECK(!g_both && !g_timeout);
		g_timeout = 1;
	}
	else if (g_down[1])
	{
		Inject(1, 1);
		CHECK(!g_both && !g_timeout);
		g_timeout = 1;
	}
	else DERROR("No buttons pressed\n");
	
	DINFO("-Timer\n");	
}

// returns 1 if the event should be dropped

static int FilterMouse(WPARAM wParam, LPARAM lParam)
{
	int btn;
	int down;
	
	switch (wParam)
	{
		case WM_LBUTTONDOWN: btn = 0; down = 1; break;
		case WM_LBUTTONUP:   btn = 0; down = 0; break;
		case WM_RBUTTONDOWN: btn = 1; down = 1; break;
		case WM_RBUTTONUP:   btn = 1; down = 0; break;
		case WM_MBUTTONDOWN: return 1;
		case WM_MBUTTONUP:   return 1;			
		default: return 0;
	}
	
	DINFO("+Filter %d %d\n", btn, down);
	
	if (!g_down[0] && !g_down[1])
	{
		CHECK(down, 0);
		g_down[btn] = 1;
		g_both = 0;
		g_timeout = 0;
		StartTimer();
	}	
	else if (down)
	{
		CHECK(!g_down[btn], 0);
		g_down[btn] = 1;
		if (!g_down[1-btn])
		{
			StartTimer();
		}
		else if (!g_both && !g_timeout)
		{
			StopTimer();
			g_both = 1;
		}
	}
	else
	{
		CHECK(g_down[btn], 0);
		g_down[btn] = 0;
		if (g_down[1-btn])
		{
			CHECK(g_both, 0);
		}
		else
		{
			if (!g_both)
			{
				StopTimer();
				Inject(btn, 1);
				Inject(btn, 0);				
			}
			else
			{
				Inject(2, 1);
				Inject(2, 0);
				g_both = 0;
			}
		}
	}

	DINFO("-Filter %d %d\n", btn, down);
	
	return 1;
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	MSLLHOOKSTRUCT *info = (MSLLHOOKSTRUCT *)lParam;

	if (nCode >= 0 && !(info->flags & LLMHF_INJECTED) && FilterMouse(wParam, lParam))
		return 1; // drop event
		
	return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

static DWORD CALLBACK Thread(LPVOID pVoid)
{
	g_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, g_hInst, 0);
	if (g_hook == NULL) {
		DERROR("SetWindowsHookEx failed: %x\n", GetLastError());
		return 0;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
		DispatchMessage(&msg);
	
	UnhookWindowsHookEx(g_hook);	
	return 0;	
}

int WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved)
{
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		g_hInst = hInstance;
		DisableThreadLibraryCalls(hInstance);
		break;
	}
	return 1;
}

int main()
{
	DWORD threadId;
	HANDLE hThread = CreateThread(NULL, 0, Thread, NULL, 0, &threadId);
	CloseHandle(hThread);
	
	eprintf("press Enter to exit\n");
	getchar();

	PostThreadMessage(threadId, WM_QUIT, 0, 0);

	return 0;
}
