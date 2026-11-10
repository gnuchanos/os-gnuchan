/*
 * wm_randr.c — keep the desktop geometry in sync with RandR changes.
 *
 * A resolution selected by the user must remain selected. The WM updates its
 * own screen geometry when RandR changes rather than restoring the mode that
 * happened to be active at session startup.
 */
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_randr.h"

static int randr_event_base = 0;
static int randr_ok = 0;

static void randr_update_desktop_size(WmCore *core) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, core->root, &attributes)) {
        return;
    }

    core->width = attributes.width;
    core->height = attributes.height;
    core->desktop_width = attributes.width;
    core->desktop_height = attributes.height;
}

static void randr_current_mode_size(WmCore *core, int *width, int *height) {
    *width = 0;
    *height = 0;

    XRRScreenResources *resources =
        XRRGetScreenResources(core->display, core->root);
    if (!resources) {
        return;
    }

    for (int i = 0; i < resources->ncrtc; i++) {
        XRRCrtcInfo *info = XRRGetCrtcInfo(core->display, resources,
                                           resources->crtcs[i]);
        if (!info) {
            continue;
        }

        if (info->mode != None) {
            for (int m = 0; m < resources->nmode; m++) {
                if (resources->modes[m].id == info->mode) {
                    if (resources->modes[m].width > 0 &&
                        resources->modes[m].height > 0) {
                        *width = (int)resources->modes[m].width;
                        *height = (int)resources->modes[m].height;
                    }
                    break;
                }
            }
        }

        XRRFreeCrtcInfo(info);
        if (*width > 0) {
            break;
        }
    }

    XRRFreeScreenResources(resources);
}

static void randr_event(WmCore *core, XEvent *event) {
    if (!randr_ok) {
        return;
    }

    int type = event->type - randr_event_base;
    if (type == RRScreenChangeNotify) {
        XRRUpdateConfiguration(event);
    }

    if (type == RRScreenChangeNotify || type == RRNotify) {
        randr_update_desktop_size(core);

        int width = 0;
        int height = 0;
        randr_current_mode_size(core, &width, &height);
        if (width > 1 && height > 1) {
            wm_frame_set_fullscreen_size(core, width, height);
        }
        return;
    }

    if (event->type == ConfigureNotify &&
        event->xconfigure.window == core->root) {
        core->width = event->xconfigure.width;
        core->height = event->xconfigure.height;
        core->desktop_width = event->xconfigure.width;
        core->desktop_height = event->xconfigure.height;
    }
}

static int randr_init(WmCore *core) {
    int error_base = 0;
    int major = 0;
    int minor = 0;

    if (!XRRQueryExtension(core->display, &randr_event_base, &error_base)) {
        return 0;
    }
    if (!XRRQueryVersion(core->display, &major, &minor) ||
        !(major > 1 || (major == 1 && minor >= 2))) {
        return 0;
    }

    XRRSelectInput(core->display, core->root,
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);
    randr_ok = 1;
    return 0;
}

static void randr_cleanup(WmCore *core) {
    (void)core;
    randr_ok = 0;
}

const WmModule wm_randr_module = {
    .name = "randr",
    .init = randr_init,
    .event = randr_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = randr_cleanup,
};
