#include <assert.h>
#include <stdio.h>

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "dm_numlock.h"

int main(void) {
    Display *display = XOpenDisplay(NULL);
    assert(display != NULL);

    KeyCode num_lock = XKeysymToKeycode(display, XK_Num_Lock);
    assert(num_lock != 0);
    XModifierKeymap *mapping = XGetModifierMapping(display);
    assert(mapping != NULL);

    unsigned int mask = 0;
    for (int modifier = 0; modifier < 8 && mask == 0; modifier++) {
        for (int key = 0; key < mapping->max_keypermod; key++) {
            if (mapping->modifiermap[modifier * mapping->max_keypermod + key] ==
                num_lock) {
                mask = 1U << modifier;
                break;
            }
        }
    }
    XFreeModifiermap(mapping);
    assert(mask != 0);

    assert(dm_numlock_enable(display));
    XkbStateRec state;
    assert(XkbGetState(display, XkbUseCoreKbd, &state) == Success);
    assert((state.locked_mods & mask) == mask);
    assert(dm_numlock_enable(display));

    XCloseDisplay(display);
    puts("Num Lock test passed");
    return 0;
}
