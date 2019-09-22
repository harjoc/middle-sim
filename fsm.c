/*
 * Copyright © 2014-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * see libinput doc/middle-button-emulation-state-machine.svg
 *
 * Note in regards to the state machine: it only handles left, right and
 * emulated middle button clicks, all other button events are passed
 * through. When in the PASSTHROUGH state, all events are passed through
 * as-is.
 */
 
#include <stdint.h>
#include "fsm.h"

#define MIDDLEBUTTON_TIMEOUT 500

#define CASE_RETURN_STRING(a) case a: return #a

enum evdev_middlebutton_state {
	MIDDLEBUTTON_IDLE,
	MIDDLEBUTTON_LEFT_DOWN,
	MIDDLEBUTTON_RIGHT_DOWN,
	MIDDLEBUTTON_MIDDLE,
	MIDDLEBUTTON_LEFT_UP_PENDING,
	MIDDLEBUTTON_RIGHT_UP_PENDING,
	MIDDLEBUTTON_IGNORE_LR,
	MIDDLEBUTTON_IGNORE_L,
	MIDDLEBUTTON_IGNORE_R,
	MIDDLEBUTTON_PASSTHROUGH,
};

enum evdev_middlebutton_event {
	MIDDLEBUTTON_EVENT_L_DOWN,
	MIDDLEBUTTON_EVENT_R_DOWN,
	MIDDLEBUTTON_EVENT_OTHER,
	MIDDLEBUTTON_EVENT_L_UP,
	MIDDLEBUTTON_EVENT_R_UP,
	MIDDLEBUTTON_EVENT_TIMEOUT,
	MIDDLEBUTTON_EVENT_ALL_UP,
};

static enum evdev_middlebutton_state g_state = MIDDLEBUTTON_IDLE;
static int g_button_mask = 0;
size_t g_timer = 0;

static void 
evdev_middlebutton_handle_timeout(HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4);

static inline const char*
middlebutton_state_to_str(enum evdev_middlebutton_state state)
{
	switch (state) {
	CASE_RETURN_STRING(MIDDLEBUTTON_IDLE);
	CASE_RETURN_STRING(MIDDLEBUTTON_LEFT_DOWN);
	CASE_RETURN_STRING(MIDDLEBUTTON_RIGHT_DOWN);
	CASE_RETURN_STRING(MIDDLEBUTTON_MIDDLE);
	CASE_RETURN_STRING(MIDDLEBUTTON_LEFT_UP_PENDING);
	CASE_RETURN_STRING(MIDDLEBUTTON_RIGHT_UP_PENDING);
	CASE_RETURN_STRING(MIDDLEBUTTON_PASSTHROUGH);
	CASE_RETURN_STRING(MIDDLEBUTTON_IGNORE_LR);
	CASE_RETURN_STRING(MIDDLEBUTTON_IGNORE_L);
	CASE_RETURN_STRING(MIDDLEBUTTON_IGNORE_R);
	}

	return NULL;
}

static inline const char*
middlebutton_event_to_str(enum evdev_middlebutton_event event)
{
	switch (event) {
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_L_DOWN);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_R_DOWN);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_OTHER);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_L_UP);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_R_UP);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_TIMEOUT);
	CASE_RETURN_STRING(MIDDLEBUTTON_EVENT_ALL_UP);
	}

	return NULL;
}

static void
middlebutton_state_error(enum evdev_middlebutton_event event)
{
	DERROR("Invalid event %s in state %s\n",
			       middlebutton_event_to_str(event),
			       middlebutton_state_to_str(g_state));
}

static void
middlebutton_timer_set()
{
	libinput_timer_set(&g_timer, evdev_middlebutton_handle_timeout, MIDDLEBUTTON_TIMEOUT);
}

static void
middlebutton_timer_cancel()
{
	libinput_timer_cancel(&g_timer);
}

static inline void
middlebutton_set_state(enum evdev_middlebutton_state state)
{
	switch (state) {
	case MIDDLEBUTTON_LEFT_DOWN:
	case MIDDLEBUTTON_RIGHT_DOWN:
		middlebutton_timer_set();
		break;
	case MIDDLEBUTTON_IDLE:
	case MIDDLEBUTTON_MIDDLE:
	case MIDDLEBUTTON_LEFT_UP_PENDING:
	case MIDDLEBUTTON_RIGHT_UP_PENDING:
	case MIDDLEBUTTON_PASSTHROUGH:
	case MIDDLEBUTTON_IGNORE_LR:
	case MIDDLEBUTTON_IGNORE_L:
	case MIDDLEBUTTON_IGNORE_R:
		middlebutton_timer_cancel();
		break;
	}

	g_state = state;
}

static void
middlebutton_post_event(int button, enum libinput_button_state state)
{
	evdev_pointer_notify_button(button, state);
}

static int
evdev_middlebutton_idle_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_set_state(MIDDLEBUTTON_LEFT_DOWN);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_set_state(MIDDLEBUTTON_RIGHT_DOWN);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
	case MIDDLEBUTTON_EVENT_L_UP:
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		break;
	}

	return 1;
}

static int
evdev_middlebutton_ldown_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_MIDDLE);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		middlebutton_post_event(BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_post_event(BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_post_event(BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
		middlebutton_set_state(MIDDLEBUTTON_IDLE);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_post_event(BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_rdown_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_MIDDLE);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		middlebutton_post_event(BTN_RIGHT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_post_event(BTN_RIGHT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_post_event(BTN_RIGHT, LIBINPUT_BUTTON_STATE_RELEASED);
		middlebutton_set_state(MIDDLEBUTTON_IDLE);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_post_event(BTN_RIGHT, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_middle_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_RELEASED);
		middlebutton_set_state(MIDDLEBUTTON_IGNORE_LR);
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_RELEASED);
		middlebutton_set_state(MIDDLEBUTTON_LEFT_UP_PENDING);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_post_event(BTN_MIDDLE,	LIBINPUT_BUTTON_STATE_RELEASED);
		middlebutton_set_state(MIDDLEBUTTON_RIGHT_UP_PENDING);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_lup_pending_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_MIDDLE);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		middlebutton_set_state(MIDDLEBUTTON_IGNORE_L);
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_set_state(MIDDLEBUTTON_IDLE);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_rup_pending_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_post_event(BTN_MIDDLE, LIBINPUT_BUTTON_STATE_PRESSED);
		middlebutton_set_state(MIDDLEBUTTON_MIDDLE);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		middlebutton_set_state(MIDDLEBUTTON_IGNORE_R);
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_set_state(MIDDLEBUTTON_IDLE);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_passthrough_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
	case MIDDLEBUTTON_EVENT_R_DOWN:
	case MIDDLEBUTTON_EVENT_OTHER:
	case MIDDLEBUTTON_EVENT_R_UP:
	case MIDDLEBUTTON_EVENT_L_UP:
		return 0;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_set_state(MIDDLEBUTTON_IDLE);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_ignore_lr_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_set_state(MIDDLEBUTTON_IGNORE_L);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_set_state(MIDDLEBUTTON_IGNORE_R);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}

static int
evdev_middlebutton_ignore_l_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		return 0;
	case MIDDLEBUTTON_EVENT_OTHER:
	case MIDDLEBUTTON_EVENT_R_UP:
		return 0;
	case MIDDLEBUTTON_EVENT_L_UP:
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		break;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
	case MIDDLEBUTTON_EVENT_ALL_UP:
		middlebutton_state_error(event);
		break;
	}

	return 1;
}
static int
evdev_middlebutton_ignore_r_handle_event(enum evdev_middlebutton_event event)
{
	switch (event) {
	case MIDDLEBUTTON_EVENT_L_DOWN:
		return 0;
	case MIDDLEBUTTON_EVENT_R_DOWN:
		middlebutton_state_error(event);
		break;
	case MIDDLEBUTTON_EVENT_OTHER:
		return 0;
	case MIDDLEBUTTON_EVENT_R_UP:
		middlebutton_set_state(MIDDLEBUTTON_PASSTHROUGH);
		break;
	case MIDDLEBUTTON_EVENT_L_UP:
		return 0;
	case MIDDLEBUTTON_EVENT_TIMEOUT:
	case MIDDLEBUTTON_EVENT_ALL_UP:
		break;
	}

	return 1;
}

static int
evdev_middlebutton_handle_event(enum evdev_middlebutton_event event)
{
	int rc = 0;
	enum evdev_middlebutton_state current;

	current = g_state;

	switch (current) {
	case MIDDLEBUTTON_IDLE:
		rc = evdev_middlebutton_idle_handle_event(event);
		break;
	case MIDDLEBUTTON_LEFT_DOWN:
		rc = evdev_middlebutton_ldown_handle_event(event);
		break;
	case MIDDLEBUTTON_RIGHT_DOWN:
		rc = evdev_middlebutton_rdown_handle_event(event);
		break;
	case MIDDLEBUTTON_MIDDLE:
		rc = evdev_middlebutton_middle_handle_event(event);
		break;
	case MIDDLEBUTTON_LEFT_UP_PENDING:
		rc = evdev_middlebutton_lup_pending_handle_event(event);
		break;
	case MIDDLEBUTTON_RIGHT_UP_PENDING:
		rc = evdev_middlebutton_rup_pending_handle_event(event);
		break;
	case MIDDLEBUTTON_PASSTHROUGH:
		rc = evdev_middlebutton_passthrough_handle_event(event);
		break;
	case MIDDLEBUTTON_IGNORE_LR:
		rc = evdev_middlebutton_ignore_lr_handle_event(event);
		break;
	case MIDDLEBUTTON_IGNORE_L:
		rc = evdev_middlebutton_ignore_l_handle_event(event);
		break;
	case MIDDLEBUTTON_IGNORE_R:
		rc = evdev_middlebutton_ignore_r_handle_event(event);
		break;
	default:
		DERROR("Invalid state %d\n", current);
		break;
	}

	DINFO("handled: %s -> %s -> %s, rc %d\n",
			middlebutton_state_to_str(current),
			middlebutton_event_to_str(event),
			middlebutton_state_to_str(g_state),
			rc);

	return rc;
}

// returns 1 if the event should be dropped

bool evdev_middlebutton_filter_button(int button, enum libinput_button_state state)
{
	enum evdev_middlebutton_event event;
	bool is_press = state == LIBINPUT_BUTTON_STATE_PRESSED;
	int rc;
	unsigned int bit = (button - BTN_LEFT);
	uint32_t old_mask = 0;

	switch (button) {
	case BTN_LEFT:
		if (is_press)
			event = MIDDLEBUTTON_EVENT_L_DOWN;
		else
			event = MIDDLEBUTTON_EVENT_L_UP;
		break;
	case BTN_RIGHT:
		if (is_press)
			event = MIDDLEBUTTON_EVENT_R_DOWN;
		else
			event = MIDDLEBUTTON_EVENT_R_UP;
		break;

	/* BTN_MIDDLE counts as "other" and resets middle button
	 * emulation */
	case BTN_MIDDLE:
	default:
		event = MIDDLEBUTTON_EVENT_OTHER;
		break;
	}

	if (button < BTN_LEFT || bit >= sizeof(g_button_mask) * 8) {
		DERROR("Button mask too small for %d\n", button);
		return 0;
	}

	rc = evdev_middlebutton_handle_event(event);

	old_mask = g_button_mask;
	if (is_press)
		g_button_mask |= 1 << bit;
	else
		g_button_mask &= ~(1 << bit);

	if (old_mask != g_button_mask && g_button_mask == 0)
		evdev_middlebutton_handle_event(MIDDLEBUTTON_EVENT_ALL_UP);

	return rc;
}

static void
evdev_middlebutton_handle_timeout(HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4)
{
	DINFO("+Timeout\n");
	evdev_middlebutton_handle_event(MIDDLEBUTTON_EVENT_TIMEOUT);
	DINFO("-Timeout\n");
}
