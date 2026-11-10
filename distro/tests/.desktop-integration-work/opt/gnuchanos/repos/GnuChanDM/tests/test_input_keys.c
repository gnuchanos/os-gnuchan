#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <X11/keysym.h>

#include "dm_core.h"

void dm_core_redraw(DmCore *core) {
    (void)core;
}

int dm_login_session_row_at(DmCore *core, int x, int y) {
    (void)core;
    (void)x;
    (void)y;
    return -1;
}

DmButton dm_login_button_at(DmCore *core, int x, int y) {
    (void)core;
    (void)x;
    (void)y;
    return DM_BUTTON_NONE;
}

int dm_login_field_at(DmCore *core, int x, int y) {
    (void)core;
    (void)x;
    (void)y;
    return -1;
}

static void press(Display *display, DmCore *core, KeySym symbol,
                  unsigned int state) {
    XKeyEvent key = {0};
    key.type = KeyPress;
    key.display = display;
    key.window = DefaultRootWindow(display);
    key.root = DefaultRootWindow(display);
    key.keycode = XKeysymToKeycode(display, symbol);
    key.state = state;
    assert(key.keycode != 0);

    XEvent event = {0};
    event.xkey = key;
    dm_input_module.event(core, &event);
}

static void type_keys(Display *display, DmCore *core, const KeySym *keys,
                      size_t count) {
    for (size_t i = 0; i < count; i++) press(display, core, keys[i], 0);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    assert(display != NULL);

    DmCore core = {0};
    core.focus = DM_FOCUS_USERNAME;
    const KeySym initial[] = {
        XK_o, XK_n, XK_e, XK_space, XK_t, XK_w, XK_o
    };
    type_keys(display, &core, initial, sizeof(initial) / sizeof(initial[0]));
    assert(strcmp(core.username, "one two") == 0);

    press(display, &core, XK_Home, 0);
    press(display, &core, XK_Right, 0);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 1);
    press(display, &core, XK_Right, ControlMask);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 4);
    press(display, &core, XK_Left, ControlMask);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 0);
    press(display, &core, XK_End, 0);
    press(display, &core, XK_BackSpace, 0);
    assert(strcmp(core.username, "one tw") == 0);
    press(display, &core, XK_Home, 0);
    press(display, &core, XK_Delete, 0);
    assert(strcmp(core.username, "ne tw") == 0);

    press(display, &core, XK_a, ControlMask);
    press(display, &core, XK_x, 0);
    assert(strcmp(core.username, "x") == 0);
    press(display, &core, XK_a, ControlMask);
    press(display, &core, XK_Delete, 0);
    assert(core.username_length == 0);

    press(display, &core, XK_Tab, 0);
    assert(core.focus == DM_FOCUS_PASSWORD);
    press(display, &core, XK_Down, 0);
    assert(core.focus == DM_FOCUS_SIGNIN);
    press(display, &core, XK_Right, 0);
    assert(core.focus == DM_FOCUS_SESSION);
    press(display, &core, XK_Left, 0);
    assert(core.focus == DM_FOCUS_SIGNIN);
    press(display, &core, XK_Up, 0);
    assert(core.focus == DM_FOCUS_PASSWORD);

    XCloseDisplay(display);
    puts("input key routing tests passed");
    return 0;
}
