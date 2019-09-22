#include <stdio.h>
#include <windows.h>
#include <winuser.h>

#include "fsm.h"

static HINSTANCE g_hInst = NULL;
static HHOOK g_hook = NULL;
static int g_in_notify = 0;

// returns 1 if the event should be dropped

static bool FilterMouse(WPARAM wParam, LPARAM lParam)
{
	MSLLHOOKSTRUCT *info = (MSLLHOOKSTRUCT *)lParam;
	int injected = info->flags & LLMHF_INJECTED;
	int button;
	int state;
	int ret;

	switch (wParam) {
	case WM_LBUTTONDOWN:
		button = BTN_LEFT;
		state = LIBINPUT_BUTTON_STATE_PRESSED;
		break;
	case WM_LBUTTONUP:
		button = BTN_LEFT;
		state = LIBINPUT_BUTTON_STATE_RELEASED;
		break;
		
	case WM_RBUTTONDOWN:
		button = BTN_RIGHT;
		state = LIBINPUT_BUTTON_STATE_PRESSED;
		break;
	case WM_RBUTTONUP:
		button = BTN_RIGHT;
		state = LIBINPUT_BUTTON_STATE_RELEASED;
		break;
		
	case WM_MBUTTONDOWN:
		button = BTN_MIDDLE;
		state = LIBINPUT_BUTTON_STATE_PRESSED;
		break;
	case WM_MBUTTONUP:
		button = BTN_MIDDLE;
		state = LIBINPUT_BUTTON_STATE_RELEASED;
		break;
	
	default:
		return 0;
	}	
	
	DINFO("+Filter: injected=%d %d %d\n", injected, button, state);
	
	ret = evdev_middlebutton_filter_button(button, state);
	
	DINFO("-Filter: injected=%d %d %d\n", injected, button, state);
	
	return ret;
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0 && FilterMouse(wParam, lParam))
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

void libinput_timer_set(size_t *timer, TIMERPROC proc, int msecs)
{
	DINFO("SetTimer\n");
	*timer = SetTimer(NULL, 0, msecs, proc);
	if (*timer == 0)
		DERROR("SetTimer failed: %x\n", GetLastError());
}

void libinput_timer_cancel(size_t *timer)
{
	DINFO("KillTimer\n");
	if (!KillTimer(NULL, *timer))
		DERROR("KillTimer failed: %x\n", GetLastError());
}

void evdev_pointer_notify_button(int button, enum libinput_button_state state)
{
	int flags;
	
	DINFO("+Notify: %d %d\n", button, state);
	
	if (button == BTN_LEFT)
		flags = (state == LIBINPUT_BUTTON_STATE_PRESSED) ? 
			MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
	else if (button == BTN_RIGHT)
		flags = (state == LIBINPUT_BUTTON_STATE_PRESSED) ? 
			MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
	else if (button == BTN_MIDDLE)
		flags = (state == LIBINPUT_BUTTON_STATE_PRESSED) ? 
			MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;			
	else {
		DERROR("Unknown button: %d\n", button);
		return;
	}
	
	INPUT input = {};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;
	
	g_in_notify = 1;
	
	if (SendInput(1, &input, sizeof(INPUT)) != 1)
		DERROR("SendInput failed: %d\n", GetLastError());
	
	g_in_notify = 0;

	DINFO("-Notify: %d %d\n", button, state);
}

int main()
{
	DWORD threadId;
	HANDLE hThread = CreateThread(NULL, 0, Thread, NULL, 0, &threadId);
	CloseHandle(hThread);
	
	getchar();

	PostThreadMessage(threadId, WM_QUIT, 0, 0);

	return 0;
}
