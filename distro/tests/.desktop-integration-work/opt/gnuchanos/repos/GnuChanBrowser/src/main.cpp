/*
 * main.cpp — start the browser on the machine it actually runs on.
 *
 * This is a 2007 laptop: an Intel 965GM whose OpenGL is desktop 2.1 and GLES
 * 2.0. Chromium — which Qt WebEngine embeds — runs only on GLES 3.0, so on
 * this GPU its compositor can never make a context and always falls back to
 * the CPU. Measured here: with the blocklist overridden and GLX named, the
 * engine still reports "Failed to initialize Skia for SharedContextState".
 * Asking it to try the GPU on every load only wastes startup and fills the
 * log; the engine is therefore pinned to software rendering, and the heavy
 * background machinery is trimmed for a two-core, low-memory machine.
 *
 * The switches are set BEFORE the QApplication is built: Chromium inside Qt
 * WebEngine reads them when it starts, and a switch set after that point is a
 * switch nothing reads. Qt reads the same switches from the environment, so
 * this is the one place they can be set for both a person running the binary
 * and a test that runs it.
 *
 *     GnuChanBrowser              the optimised software profile below
 *     GnuChanBrowser --software   accepted for compatibility; the same thing
 */
#include "browser_theme.h"
#include "browser_window.h"

#include <QApplication>
#include <QByteArray>

int main(int argc, char **argv) {
    /* The engine wants this for a session that runs the setuid-free binary as
       a normal user without a sandbox helper installed. */
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", QByteArrayLiteral("1"));

    QByteArray flags;

    /* No GPU process at all - and that is the point, not a side effect.

       The wrong turn was to keep a GPU process alive and point it at ANGLE on
       SwiftShader, hoping for software WebGL. It ran, and it cost more than
       it gave: this machine's software GL context kept being lost (thousands
       of "CreateSharedImage failed"), so the process never settled and the
       browser burned about 180% of the CPU while sitting on an empty page.
       That is worse than the feature was worth.

       `--disable-gpu` starts no GPU process, so there is nothing left to spin.
       The pages are composited by the processor directly, the browser is quiet
       when idle, and this machine's real GPU — which Chromium cannot use
       anyway, since it needs GLES 3.0 and this chip gives 2.0 — is never in
       the path and cannot hang. The cost is WebGL: with no GPU process it is
       simply off. On a 2007 laptop that is the right trade, and it is the one
       that keeps the fans off. */
    flags +=
        "--disable-gpu "
        "--disable-gpu-compositing "
        "--disable-gpu-rasterization "
        "--disable-accelerated-2d-canvas "
        "--disable-accelerated-video-decode "
        "--disable-accelerated-video-encode ";

    /* Avoid starving large web applications such as chat clients. A single
       renderer and a fixed V8 heap cap made complex sites stall or fail to
       finish loading. Keep raster work bounded for the old two-core machine,
       but let Chromium manage renderer processes and page memory. */
    flags +=
        "--num-raster-threads=1 "
        "--disable-dev-shm-usage ";

    flags +=
        "--disable-features=BackForwardCache,Translate,OptimizationHints,"
        "MediaRouter ";

    /* Keep Chromium's network service isolated and its hang recovery enabled.
       Folding networking into the UI process or disabling the hang monitor
       makes a slow/heavy page more likely to look like a frozen browser. */
    flags +=
        "--disable-crash-reporter "
        "--no-pings "
        "--disable-prompt-on-repost ";

    /* Less happening behind the window, which is CPU the page never gets.
       None of these change what a page shows; they stop background network,
       update checks, and metrics. */
    flags +=
        "--disable-background-networking "
        "--disable-component-update "
        "--disable-domain-reliability "
        "--disable-client-side-phishing-detection "
        "--disable-sync "
        "--disable-default-apps "
        "--disable-extensions "
        "--disable-translate "
        "--no-first-run "
        "--no-default-browser-check "
        "--metrics-recording-only "
        "--disable-breakpad";

    /* The engine's own logging, quieted. Chromium logs to stderr on ordinary
       events — a failed subresource, a GPU fallback it repeats, a network
       line — and every one of those is a write and a flush the browser pays
       for and the user never asked for. --log-level=3 keeps FATAL only, and
       --disable-logging stops the rest; the browser's OWN two lines per page
       (loaded / download) still reach stderr, because those are written by
       this program and not by the engine. The page's own console is quieted
       separately, in browser_window.cpp. */
    flags +=
        " --log-level=3 "
        "--disable-logging";

    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("GnuChanBrowser"));
    /* One style sheet for the whole window — the GnuchanOS violet — so every
       widget in the browser is painted from the same palette. */
    application.setStyleSheet(browser_style_sheet());

    BrowserWindow window;
    window.show();

    return application.exec();
}
