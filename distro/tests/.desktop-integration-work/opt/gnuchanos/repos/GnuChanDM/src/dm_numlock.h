#ifndef GNUCHANDM_NUMLOCK_H
#define GNUCHANDM_NUMLOCK_H

#include <X11/Xlib.h>

/* Best-effort: enable the modifier used by the Num Lock keysym. */
int dm_numlock_enable(Display *display);

#endif
