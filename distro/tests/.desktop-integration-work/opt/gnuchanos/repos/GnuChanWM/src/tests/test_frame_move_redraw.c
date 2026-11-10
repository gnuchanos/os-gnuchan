#include "../wm_core.h"
#include "../wm_frame.h"

#include <assert.h>
#include <X11/Xutil.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    assert(display != NULL);

    WmCore core = {0};
    core.display = display;
    core.screen = DefaultScreen(display);
    core.root = RootWindow(display, core.screen);
    core.gc = XCreateGC(display, core.root, 0, NULL);
    core.style.border_unfocused = 0xff0000;
    core.style.panel_unfocused = 0x202020;
    core.style.panel = 0x303030;
    core.style.field = 0x404040;
    core.style.accent = 0xffffff;
    core.style.text = 0xffffff;

    WmFrame frame = {0};
    frame.x = 20;
    frame.y = 20;
    frame.border = 2;
    frame.client_width = 100;
    frame.client_height = 50;
    frame.frame = XCreateSimpleWindow(display, core.root, frame.x, frame.y,
                                      104, 74, 0, 0, 0);
    frame.client = XCreateSimpleWindow(display, frame.frame, 2, WM_TITLE_HEIGHT,
                                       100, 50, 0, 0, 0);
    XMapWindow(display, frame.client);
    XMapWindow(display, frame.frame);
    XSync(display, False);

    GC damage_gc = XCreateGC(display, frame.frame, 0, NULL);
    XSetForeground(display, damage_gc, 0);
    XFillRectangle(display, frame.frame, damage_gc, 0, WM_TITLE_HEIGHT,
                   2, 20);
    XSync(display, False);

    wm_frame_move(&core, &frame, 30, 30);
    XSync(display, False);

    XImage *image = XGetImage(display, frame.frame, 0, WM_TITLE_HEIGHT,
                              1, 1, AllPlanes, ZPixmap);
    assert(image != NULL);
    assert(XGetPixel(image, 0, 0) == core.style.border_unfocused);

    XDestroyImage(image);
    XFreeGC(display, damage_gc);
    XDestroyWindow(display, frame.frame);
    XFreeGC(display, core.gc);
    XCloseDisplay(display);
    return 0;
}
