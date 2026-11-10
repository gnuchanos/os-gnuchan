#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "dm_numlock.h"

int dm_numlock_enable(Display *display) {
    if (!display) return 0;

    KeyCode num_lock = XKeysymToKeycode(display, XK_Num_Lock);
    if (num_lock == 0) return 0;

    XModifierKeymap *mapping = XGetModifierMapping(display);
    if (!mapping) return 0;

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
    if (mask == 0) return 0;

    int opcode, event, error;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;
    if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
        return 0;
    }

    if (!XkbLockModifiers(display, XkbUseCoreKbd, mask, mask)) return 0;
    XSync(display, False);
    return 1;
}
