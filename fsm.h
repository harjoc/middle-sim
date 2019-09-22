#pragma once

#include <stdio.h> // fprintf
#include <Windows.h> // TIMERPROC

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define DINFO  eprintf
#define DERROR eprintf

enum libinput_button_state {
	LIBINPUT_BUTTON_STATE_RELEASED = 0,
	LIBINPUT_BUTTON_STATE_PRESSED = 1
};

#define BTN_LEFT		0
#define BTN_RIGHT		1
#define BTN_MIDDLE		2

typedef int bool;

void libinput_timer_set(size_t *g_timer, TIMERPROC proc, int msecs);
void libinput_timer_cancel(size_t *g_timer);

bool evdev_middlebutton_filter_button(int button, enum libinput_button_state state);

void evdev_pointer_notify_button(int button, enum libinput_button_state state);
