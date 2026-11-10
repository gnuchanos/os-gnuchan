/*
 * GnuChanSoftwareCenter.c — GnuchanOS software center.
 *
 * Linux Mint App Market benzeri bir arayuz: Debian paketlerini listeler,
 * yuklenmis paketleri goster, guncellemeleri kontrol eder ve GnuChan
 * X11 programlarini otomatik olarak gunceller.
 *
 * Kurulu GnuChan programlarinin GitHub version.txt dosyalarini yerel
 * config versiyonlariyla karsilastirir. Paket taramasi ve guncelleme
 * denetimleri arayuz acildiktan sonra arka planda yapilir.
 *
 * Islevler:
 *   - Debian depo katalogunu apt-cache ile listeleme
 *   - Yuklenmis paket durumunu dpkg-query ile isaretleme
 *   - Guncelleme denetimi (apt-get -s upgrade)
 *   - Tekli paket kurma / kaldirma (apt-get install / remove)
 *   - Tum guncellemeleri tek seferde uygulama
 *   - GnuChan programlarini GitHub'dan versiyon denetimi ile guncelleme
 *   - Arama kutusu ve kategori secimi (Tum, Yuklenmis, Guncellemeler, GnuChan)
 *
 * Arka uclarda popen() ile dpkg-query, apt-cache, apt-get, curl cagrilir.
 * Arayuz sade X11 cizimleri ile yapilir.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "gnuchan_lang.h"

#define GNUCHANSOFTWARECENTER_VERSION "0.3.0"

/* --- boyutlar ------------------------------------------------------------ */

#define INITIAL_WIN_WIDTH  1000
#define INITIAL_WIN_HEIGHT 700

static int window_width = INITIAL_WIN_WIDTH;
static int window_height = INITIAL_WIN_HEIGHT;

#define WIN_WIDTH  window_width
#define WIN_HEIGHT window_height

#define SIDEBAR_W  220
#define HEADER_H   44
#define FOOTER_H   144
#define ITEM_H     26
#define LIST_TOP   (HEADER_H + 10)
#define LIST_LEFT  (SIDEBAR_W + 10)
#define LIST_RIGHT (WIN_WIDTH - 25)
#define LIST_BOTTOM (WIN_HEIGHT - FOOTER_H - 10)
#define SCROLLBAR_X (WIN_WIDTH - 18)
#define SCROLLBAR_W 8

#define MAX_PACKAGES 131072
#define PACKAGE_HASH_SIZE 262144
#define MAX_NAME_LEN 256
#define MAX_VERSION_LEN 64
#define MAX_DESC_LEN 512
#define MAX_TAGS_LEN 2048
#define MAX_METADATA_LEN 160
#define MAX_DEPENDS_LEN 512
#define CMD_BUF_LEN 8192
#define LINE_BUF_LEN 2048
#define CONFIG_PATH_LEN 512
#define MAX_QUEUE_ITEMS 128
#define MAX_FLATPAK_REMOTES 64

/* --- renkler ------------------------------------------------------------- */

#define COL_BG        0x160D20U
#define COL_SIDEBAR   0x21102FU
#define COL_HEADER    0x241433U
#define COL_FOOTER    0x241433U
#define COL_TEXT      0xF5EAFFU
#define COL_TEXT_DIM  0xB9A2C9U
#define COL_BORDER    0x513768U
#define COL_SELECT    0x392150U
#define COL_BUTTON    0x8B5CB8U
#define COL_BUTTON_H  0xA572D1U
#define COL_DANGER    0x75478FU
#define COL_SUCCESS   0x8B5CB8U
#define COL_WARN      0xB78ED6U
#define COL_UPDATE    0xA855F7U

/* --- paket modeli -------------------------------------------------------- */

typedef enum PkgState {
    PKG_STATE_UNKNOWN,
    PKG_STATE_INSTALLED,
    PKG_STATE_NOT_INSTALLED,
} PkgState;

typedef enum PackageCategory {
    CATEGORY_SYSTEM,
    CATEGORY_INTERNET,
    CATEGORY_GRAPHICS,
    CATEGORY_MULTIMEDIA,
    CATEGORY_DEVELOPMENT,
    CATEGORY_EDUCATION,
    CATEGORY_SCIENCE,
    CATEGORY_GAMES,
    CATEGORY_BROWSER,
    CATEGORY_FONTS,
    CATEGORY_OTHER,
} PackageCategory;

typedef enum PackageSource {
    SOURCE_APT,
    SOURCE_FLATPAK,
} PackageSource;

typedef enum QueueAction {
    QUEUE_INSTALL,
    QUEUE_REMOVE,
    QUEUE_UPDATE,
    QUEUE_UPDATE_ALL,
    QUEUE_GNUCHAN_UPDATE,
} QueueAction;

typedef struct Package {
    char name[MAX_NAME_LEN];
    char version[MAX_VERSION_LEN];
    char remote_version[MAX_VERSION_LEN];
    char desc[MAX_DESC_LEN];
    char maintainer[MAX_METADATA_LEN];
    char homepage[MAX_METADATA_LEN];
    char depends[MAX_DEPENDS_LEN];
    char license[MAX_METADATA_LEN];
    char origin[MAX_METADATA_LEN];
    unsigned long installed_size_kb;
    unsigned long download_size_bytes;
    PackageSource source;
    PkgState state;
    PackageCategory category;
    int is_gnuchan;      /* 1: GnuChan programi */
    int has_update;      /* 1: guncelleme mevcut (apt veya github) */
    int apt_update;      /* 1: apt tarafinda guncelleme mevcut */
    int github_update;   /* 1: GitHub'da yeni versiyon var */
    int selected;        /* UI secim durumu */
    struct Package *next;
} Package;

static Package *pkg_head = NULL;
static int pkg_count = 0;
static Package *pkg_hash[PACKAGE_HASH_SIZE];

/* --- UI durumu ----------------------------------------------------------- */

typedef enum ViewId {
    VIEW_ALL,
    VIEW_INSTALLED,
    VIEW_UPDATES,
    VIEW_GNUCHAN,
    VIEW_SYSTEM,
    VIEW_INTERNET,
    VIEW_GRAPHICS,
    VIEW_MULTIMEDIA,
    VIEW_DEVELOPMENT,
    VIEW_EDUCATION,
    VIEW_SCIENCE,
    VIEW_GAMES,
    VIEW_BROWSER,
    VIEW_FONTS,
} ViewId;

static ViewId current_view = VIEW_ALL;
static char search_text[256] = {0};
static int search_focused = 0;
static int scroll_offset = 0;
static int scrollbar_dragging = 0;
static int selected_index = -1;
static char status_msg[512] = {0};
static int busy = 0;
static int update_available = 0;
static int gnuchan_update_count = 0;
static int github_check_done = 0;
static int github_check_failures = 0;
static int show_installed_by_default = 0;
static int check_updates_on_start = 1;
static int require_authentication = 1;
static int update_checks_disabled = 0;
static PackageSource current_source = SOURCE_APT;
static int flatpak_available = 0;
static char flatpak_remotes[MAX_FLATPAK_REMOTES][MAX_METADATA_LEN];
static int flatpak_remote_count = 0;
static char source_status[256] = {0};
static char progress_status[256] = {0};
static int progress_percent = -1;
static int queue_count = 0;
static int queue_head = 0;
static int queue_total = 0;
static QueueAction operation_queue[MAX_QUEUE_ITEMS];
static PackageSource operation_queue_source[MAX_QUEUE_ITEMS];
static char operation_queue_package[MAX_QUEUE_ITEMS][MAX_NAME_LEN];
static char operation_queue_remote[MAX_QUEUE_ITEMS][MAX_METADATA_LEN];
static int queue_worker_running = 0;
static int details_loading = 0;
static int queue_completed = 0;
static int detail_view = 0;
static int loading_packages = 1;
static int checking_updates = 0;
static unsigned long state_revision = 0;
static pthread_mutex_t state_mutex;
static Pixmap back_buffer = None;
static int back_buffer_width = 0;
static int back_buffer_height = 0;

static volatile sig_atomic_t should_stop = 0;
static void load_installed_packages(void);
static void add_user_gnuchan_programs(const char *home);
static void rebuild_visible_list(void);
static int source_selector_x(void);
static int bulk_update_button_x(int has_selected_package);
static int run_queue_button_x(int has_selected_package);
static int gnuchan_update_button_x(int has_selected_package);
static int valid_flatpak_token(const char *token);

/* --- GnuChan program listesi (repo adi -> program adi) ------------------ */

typedef struct GnuchanRepo {
    char repo[128];      /* GitHub repo adi (gnuchanos/<repo>) */
    char program[128];   /* Program adi */
    char config_dir[128]; /* ~/.config/<config_dir>/ */
    char config_file[128]; /* Config dosya adi */
    char legacy_config_file[128];
    char local_version[MAX_VERSION_LEN];
    char remote_version[MAX_VERSION_LEN];
    int has_remote;
    int update_available;
} GnuchanRepo;

static GnuchanRepo gnuchan_repos[] = {
    {.repo = "GnuChanWM", .program = "GnuChanWM", .config_dir = "GnuChanWM",
     .config_file = "GnuChanWM.conf", .legacy_config_file = "GnuChanWM.py"},
    {.repo = "GnuChanSS", .program = "GnuChanSS", .config_dir = "GnuChanSS",
     .config_file = "GnuChanSS.conf", .legacy_config_file = "GnuChanSS.py"},
    {.repo = "GnuChanSettings", .program = "GnuChanSettings",
     .config_dir = "GnuChanSettings", .config_file = "system.conf"},
    {.repo = "GnuChanDock", .program = "GnuChanDock",
     .config_dir = "GnuChanDock", .config_file = "GnuChanDock.conf",
     .legacy_config_file = "GnuChanDock.py"},
    {.repo = "GnuChanFetch", .program = "GnuChanFetch",
     .config_dir = "GnuChanFetch", .config_file = "GnuChanFetch.conf",
     .legacy_config_file = "GnuChanFetch.py"},
    {.repo = "GnuChanTerm", .program = "GnuChanTerm", .config_dir = "GnuChanTerm",
     .config_file = "GnuChanTerm.conf",
     .legacy_config_file = "GnuChanTerm.py"},
    {.repo = "GnuChanTop", .program = "GnuChanTop", .config_dir = "GnuChanTop",
     .config_file = "GnuChanTop.conf",
     .legacy_config_file = "GnuChanTop.py"},
    {.repo = "GnuChanWifi", .program = "GnuChanWifi", .config_dir = "GnuChanWifi",
     .config_file = "config.conf", .legacy_config_file = "GnuChanWifi.py"},
    {.repo = "GnuChanLockScreen", .program = "GnuChanLockScreen",
     .config_dir = "GnuChanSL", .config_file = "GnuChanSL.conf",
     .legacy_config_file = "GnuChanSL.py"},
    {.repo = "GnuChanNetworkManager", .program = "GnuChanNetworkManager",
     .config_dir = "GnuChanNetworkManager", .config_file = "config.conf",
     .legacy_config_file = "config.py"},
    {.repo = "GnuChanNotification", .program = "GnuChanNotification",
     .config_dir = "GnuChanNotification",
     .config_file = "GnuChanNotification.conf",
     .legacy_config_file = "GnuChanNotification.py"},
    {.repo = "GnuChanRunner", .program = "GnuChanRunner",
     .config_dir = "GnuChanRunner", .config_file = "config.conf",
     .legacy_config_file = "config.py"},
    {.repo = "GnuChanScreenShoot", .program = "GnuChanScreenShoot",
     .config_dir = "GnuChanScreenShoot",
     .config_file = "GnuChanScreenShoot.conf",
     .legacy_config_file = "GnuChanScreenShoot.py"},
    {.repo = "GnuChanSoftwareCenter", .program = "GnuChanSoftwareCenter",
     .config_dir = "GnuChanSoftwareCenter",
     .config_file = "GnuChanSoftwareCenter.conf"},
    {.repo = "GnuChanBrowser", .program = "GnuChanBrowser",
     .config_dir = "GnuChanBrowser", .config_file = "GnuChanBrowser.conf",
     .legacy_config_file = "config.py"},
    {.repo = "GnuChanDM", .program = "GnuChanDM", .config_dir = "GnuChanDM",
     .config_file = "GnuChanDM.conf", .legacy_config_file = "GnuChanDM.py"},
};
#define GNUCHAN_REPO_COUNT (sizeof(gnuchan_repos) / sizeof(gnuchan_repos[0]))

/* --- yardimcilar --------------------------------------------------------- */

static void set_status(const char *msg) {
    snprintf(status_msg, sizeof(status_msg), "%s", msg);
}

static const char *center_text(const char *english, const char *turkish) {
    return gnuchan_lang_is_turkish() ? turkish : english;
}

static void set_status_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
    va_end(ap);
}

static int parse_setting_bool(const char *value, int fallback) {
    if (strcmp(value, "True") == 0 || strcmp(value, "true") == 0) return 1;
    if (strcmp(value, "False") == 0 || strcmp(value, "false") == 0) return 0;
    return fallback;
}

static void apply_center_setting(const char *key, const char *value) {
    if (strcmp(key, "ShowInstalled") == 0) {
        show_installed_by_default = parse_setting_bool(
            value, show_installed_by_default);
    } else if (strcmp(key, "CheckUpdates") == 0) {
        check_updates_on_start = parse_setting_bool(
            value, check_updates_on_start);
    } else if (strcmp(key, "RequireAuth") == 0) {
        require_authentication = parse_setting_bool(
            value, require_authentication);
    } else if (strcmp(key, "Source") == 0) {
        current_source = strcmp(value, "flatpak") == 0
                             ? SOURCE_FLATPAK : SOURCE_APT;
    }
}

static void parse_center_settings_line(char *line) {
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';

    /* Accept flat .conf lines and GnuChanSettings' non-executed call format:
       gcl_SoftwareCenter.Settings(ShowInstalled=True, ...). */
    for (char *equals = strchr(line, '='); equals;
         equals = strchr(equals + 1, '=')) {
        char *key_end = equals;
        while (key_end > line && isspace((unsigned char)key_end[-1])) {
            key_end--;
        }
        char *key_start = key_end;
        while (key_start > line &&
               (isalnum((unsigned char)key_start[-1]) ||
                key_start[-1] == '_')) {
            key_start--;
        }
        size_t key_length = (size_t)(key_end - key_start);
        if (key_length == 0 || key_length >= 64) continue;

        char key[64], value[128];
        memcpy(key, key_start, key_length);
        key[key_length] = '\0';

        const char *value_start = equals + 1;
        while (isspace((unsigned char)*value_start)) value_start++;
        size_t value_length = strcspn(value_start, ", )\t\r\n");
        if (value_length >= 2 &&
            ((value_start[0] == '"' &&
              value_start[value_length - 1] == '"') ||
             (value_start[0] == '\'' &&
              value_start[value_length - 1] == '\''))) {
            value_start++;
            value_length -= 2;
        }
        if (value_length == 0 || value_length >= sizeof(value)) continue;
        memcpy(value, value_start, value_length);
        value[value_length] = '\0';
        apply_center_setting(key, value);
    }
}

static void load_center_settings(const char *home) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char settings_path[CONFIG_PATH_LEN], config_path[CONFIG_PATH_LEN];
    if (xdg && xdg[0]) {
        snprintf(settings_path, sizeof(settings_path),
                 "%s/GnuChanSoftwareCenter/GnuChanSoftwareCenter.py", xdg);
        snprintf(config_path, sizeof(config_path),
                 "%s/GnuChanSoftwareCenter/GnuChanSoftwareCenter.conf", xdg);
    } else {
        snprintf(settings_path, sizeof(settings_path),
                 "%s/.config/GnuChanSoftwareCenter/GnuChanSoftwareCenter.py",
                 home);
        snprintf(config_path, sizeof(config_path),
                 "%s/.config/GnuChanSoftwareCenter/GnuChanSoftwareCenter.conf",
                 home);
    }

    /* The flat config is authoritative; the non-executed .py parser remains
       only as a compatibility fallback for older installations. */
    FILE *f = fopen(config_path, "r");
    if (!f) f = fopen(settings_path, "r");
    if (!f) return;

    char line[LINE_BUF_LEN];
    while (fgets(line, sizeof(line), f)) {
        parse_center_settings_line(line);
    }
    fclose(f);
}

static int run_cmd(const char *cmd, char *out, size_t out_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(out, out_size, "popen failed: %s", strerror(errno));
        return -1;
    }
    size_t n = 0;
    char line[LINE_BUF_LEN];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (n + len < out_size) {
            memcpy(out + n, line, len);
            n += len;
        }
        char *text = line;
        while (isspace((unsigned char)*text)) text++;
        char *end = text + strlen(text);
        while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
        if (text[0]) {
            pthread_mutex_lock(&state_mutex);
            if (queue_worker_running) {
                snprintf(progress_status, sizeof(progress_status), "%.250s", text);
                char *percent = strstr(text, "pmstatus:");
                if (percent) {
                    percent = strchr(percent, ':');
                    if (percent) {
                        percent = strchr(percent + 1, ':');
                        if (percent) {
                            int value = atoi(percent + 1);
                            if (value >= 0 && value <= 100) {
                                progress_percent = value;
                            }
                        }
                    }
                }
                state_revision++;
            }
            pthread_mutex_unlock(&state_mutex);
        }
    }
    out[n] = '\0';
    int rc = pclose(fp);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

static void check_apt_updates(void);
static void check_github_versions(const char *home);

static void refresh_update_checks(const char *home) {
    if (!check_updates_on_start) {
        pthread_mutex_lock(&state_mutex);
        update_checks_disabled = 1;
        github_check_done = 1;
        update_available = 0;
        gnuchan_update_count = 0;
        set_status(center_text("Automatic update checks are disabled in settings",
                               "Otomatik guncelleme denetimleri ayarlarda kapali"));
        checking_updates = 0;
        state_revision++;
        pthread_mutex_unlock(&state_mutex);
        return;
    }
    pthread_mutex_lock(&state_mutex);
    update_checks_disabled = 0;
    checking_updates = 1;
    github_check_done = 0;
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
    check_apt_updates();
    check_github_versions(home);
}

/* --- versiyon karsilastirma ---------------------------------------------- */

/* "0.1.0" -> 10000, "0.2.0" -> 20000, "1.10.3" -> 101003 */
static int version_to_int(const char *v) {
    int major = 0, minor = 0, patch = 0;
    sscanf(v, "%d.%d.%d", &major, &minor, &patch);
    return major * 1000000 + minor * 1000 + patch;
}

static int version_newer(const char *a, const char *b) {
    return version_to_int(a) > version_to_int(b);
}

static int version_valid(const char *version) {
    int major, minor, patch;
    char extra;
    return version && strlen(version) < MAX_VERSION_LEN &&
           sscanf(version, "%d.%d.%d%c",
                             &major, &minor, &patch, &extra) == 3 &&
           major >= 0 && minor >= 0 && patch >= 0;
}

/* --- config dosyasindan versiyon okuma ---------------------------------- */

static int repo_config_path(const char *home, const GnuchanRepo *repo,
                            char *path, size_t path_size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *config_home = xdg && xdg[0] == '/'
                                  ? xdg : NULL;
    int written = config_home
        ? snprintf(path, path_size, "%s/%s/%s", config_home,
                   repo->config_dir, repo->config_file)
        : snprintf(path, path_size, "%s/.config/%s/%s", home,
                   repo->config_dir, repo->config_file);
    if (written < 0 || (size_t)written >= path_size) return 0;
    if (access(path, R_OK) == 0 || !repo->legacy_config_file[0]) return 1;

    written = config_home
        ? snprintf(path, path_size, "%s/%s/%s", config_home,
                   repo->config_dir, repo->legacy_config_file)
        : snprintf(path, path_size, "%s/.config/%s/%s", home,
                   repo->config_dir, repo->legacy_config_file);
    return written >= 0 && (size_t)written < path_size;
}

static void read_config_version(const char *home, const GnuchanRepo *repo,
                                char *out,
                                size_t out_size) {
    char path[CONFIG_PATH_LEN];
    if (!repo_config_path(home, repo, path, sizeof(path))) {
        snprintf(out, out_size, "0.0.0");
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out, out_size, "0.0.0");
        return;
    }

    char line[LINE_BUF_LEN];
    out[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        /* Version = "0.1.0"  veya  Version="0.1.0" */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        char *key = line;
        while (*key && (*key == ' ' || *key == '\t')) key++;
        char *ke_end = eq - 1;
        while (ke_end > key && (*ke_end == ' ' || *ke_end == '\t')) ke_end--;
        *(ke_end + 1) = '\0';

        if (strcmp(key, "Version") != 0) continue;

        /* value parala */
        char *val = eq + 1;
        while (*val && (*val == ' ' || *val == '\t')) val++;
        if (*val == '"') val++;
        char *end = val;
        while (*end && *end != '"' && *end != '\n' && *end != '\r') end++;
        *end = '\0';
        snprintf(out, out_size, "%s", val);
        break;
    }
    fclose(f);
    if (out[0] == '\0') {
        snprintf(out, out_size, "0.0.0");
    }
}

/* --- paket veritabani ---------------------------------------------------- */

static void pkg_free_all(void) {
    Package *p = pkg_head;
    while (p) {
        Package *n = p->next;
        free(p);
        p = n;
    }
    pkg_head = NULL;
    pkg_count = 0;
    memset(pkg_hash, 0, sizeof(pkg_hash));
}

static size_t pkg_hash_index(const char *name) {
    unsigned long hash = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        hash = (hash ^ *p) * 16777619UL;
    }
    return (size_t)(hash & (PACKAGE_HASH_SIZE - 1));
}

static Package *pkg_find(const char *name) {
    size_t index = pkg_hash_index(name);
    for (size_t probed = 0; probed < PACKAGE_HASH_SIZE; probed++) {
        Package *p = pkg_hash[index];
        if (!p) return NULL;
        if (strcmp(p->name, name) == 0) return p;
        index = (index + 1) & (PACKAGE_HASH_SIZE - 1);
    }
    return NULL;
}

static void pkg_add(const char *name, const char *version,
                    const char *desc, PkgState state,
                    PackageCategory category,
                    int is_gnuchan, int has_update, int github_update) {
    Package *existing = pkg_find(name);
    if (existing) {
        if (existing->state != PKG_STATE_INSTALLED ||
            state == PKG_STATE_INSTALLED) {
            if (version && version[0]) {
                snprintf(existing->version, sizeof(existing->version), "%s",
                         version);
            }
        }
        if (desc && desc[0]) {
            snprintf(existing->desc, sizeof(existing->desc), "%s", desc);
        }
        existing->category = category;
        existing->is_gnuchan |= is_gnuchan;
        if (state == PKG_STATE_INSTALLED) {
            existing->state = PKG_STATE_INSTALLED;
        }
        existing->has_update |= has_update;
        existing->github_update |= github_update;
        return;
    }
    if (pkg_count >= MAX_PACKAGES) return;
    Package *p = calloc(1, sizeof(*p));
    if (!p) return;
    snprintf(p->name, sizeof(p->name), "%s", name);
    snprintf(p->version, sizeof(p->version), "%s", version ? version : "");
    snprintf(p->remote_version, sizeof(p->remote_version), "%s", version ? version : "");
    snprintf(p->desc, sizeof(p->desc), "%s", desc ? desc : "");
    p->state = state;
    p->source = SOURCE_APT;
    p->category = category;
    p->is_gnuchan = is_gnuchan;
    p->has_update = has_update;
    p->github_update = github_update;
    p->next = pkg_head;
    pkg_head = p;
    size_t index = pkg_hash_index(name);
    while (pkg_hash[index]) {
        index = (index + 1) & (PACKAGE_HASH_SIZE - 1);
    }
    pkg_hash[index] = p;
    pkg_count++;
}

static int is_gnuchan_binary(const char *name) {
    if (strncmp(name, "gnuchan", 7) == 0) return 1;
    if (strncmp(name, "GnuChan", 7) == 0) return 1;
    if (strcmp(name, "gcl") == 0) return 1;
    return 0;
}

static PackageCategory category_for_section(const char *section) {
    const char *base_section = strrchr(section, '/');
    if (base_section) section = base_section + 1;
    if (strcmp(section, "fonts") == 0) return CATEGORY_FONTS;
    if (strncmp(section, "games", 5) == 0) return CATEGORY_GAMES;
    if (strncmp(section, "net", 3) == 0 ||
        strncmp(section, "web", 3) == 0 ||
        strncmp(section, "mail", 4) == 0) {
        return CATEGORY_INTERNET;
    }
    if (strncmp(section, "comm", 4) == 0 ||
        strncmp(section, "news", 4) == 0 ||
        strncmp(section, "hamradio", 8) == 0 ||
        strncmp(section, "httpd", 5) == 0) {
        return CATEGORY_INTERNET;
    }
    if (strncmp(section, "graphics", 8) == 0 ||
        strncmp(section, "x11", 3) == 0 ||
        strncmp(section, "gnome", 5) == 0 ||
        strncmp(section, "kde", 3) == 0) {
        return CATEGORY_GRAPHICS;
    }
    if (strncmp(section, "sound", 5) == 0 ||
        strncmp(section, "video", 5) == 0) {
        return CATEGORY_MULTIMEDIA;
    }
    if (strncmp(section, "devel", 5) == 0 ||
        strncmp(section, "lib", 3) == 0 ||
        strncmp(section, "debug", 5) == 0 ||
        strncmp(section, "interpreters", 12) == 0 ||
        strncmp(section, "haskell", 7) == 0 ||
        strncmp(section, "java", 4) == 0 ||
        strncmp(section, "javascript", 10) == 0 ||
        strncmp(section, "lisp", 4) == 0 ||
        strncmp(section, "ocaml", 5) == 0 ||
        strncmp(section, "perl", 4) == 0 ||
        strncmp(section, "php", 3) == 0 ||
        strncmp(section, "python", 6) == 0 ||
        strncmp(section, "ruby", 4) == 0 ||
        strncmp(section, "rust", 4) == 0 ||
        strncmp(section, "vcs", 3) == 0 ||
        strncmp(section, "tex", 3) == 0 ||
        strncmp(section, "editors", 7) == 0) {
        return CATEGORY_DEVELOPMENT;
    }
    if (strncmp(section, "education", 9) == 0) {
        return CATEGORY_EDUCATION;
    }
    if (strncmp(section, "science", 7) == 0 ||
        strncmp(section, "math", 4) == 0) {
        return CATEGORY_SCIENCE;
    }
    if (strncmp(section, "admin", 5) == 0 ||
        strncmp(section, "base", 4) == 0 ||
        strncmp(section, "utils", 5) == 0 ||
        strncmp(section, "kernel", 6) == 0 ||
        strncmp(section, "shells", 6) == 0 ||
        strncmp(section, "metapackages", 12) == 0 ||
        strncmp(section, "tasks", 5) == 0 ||
        strncmp(section, "oldlibs", 7) == 0 ||
        strncmp(section, "otherosfs", 9) == 0 ||
        strncmp(section, "doc", 3) == 0 ||
        strncmp(section, "fonts", 5) == 0 ||
        strncmp(section, "localization", 12) == 0 ||
        strncmp(section, "text", 4) == 0 ||
        strncmp(section, "misc", 4) == 0) {
        return CATEGORY_SYSTEM;
    }
    return CATEGORY_OTHER;
}

static int text_contains_case_insensitive(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) return 0;
    for (const char *start = text; *start; start++) {
        const char *left = start;
        const char *right = needle;
        while (*left && *right &&
               tolower((unsigned char)*left) ==
                   tolower((unsigned char)*right)) {
            left++;
            right++;
        }
        if (!*right) return 1;
    }
    return 0;
}

static int tag_list_contains(const char *tags, const char *tag) {
    size_t tag_length = strlen(tag);
    for (const char *current = tags; current && *current;) {
        while (*current &&
               (isspace((unsigned char)*current) || *current == ',')) {
            current++;
        }
        const char *end = current;
        while (*end && !isspace((unsigned char)*end) && *end != ',') end++;
        if ((size_t)(end - current) == tag_length &&
            strncmp(current, tag, tag_length) == 0) {
            return 1;
        }
        current = end;
    }
    return 0;
}

static int is_browser_package(const char *description, const char *section,
                              const char *tags) {
    const char *base_section = strrchr(section, '/');
    if (base_section) section = base_section + 1;
    if (tag_list_contains(tags, "web::browser") ||
        tag_list_contains(tags, "use::browsing")) {
        return 1;
    }
    return strncmp(section, "web", 3) == 0 &&
           text_contains_case_insensitive(description, "browser");
}

static void add_catalog_package(const char *name, const char *version,
                                const char *description,
                                const char *section, const char *tags,
                                const char *maintainer, const char *homepage,
                                const char *depends, const char *license,
                                unsigned long installed_size_kb,
                                unsigned long download_size_bytes) {
    if (!name[0]) return;
    PackageCategory category = category_for_section(section);
    if (tag_list_contains(tags, "role::font") ||
        tag_list_contains(tags, "use::font")) {
        category = CATEGORY_FONTS;
    }
    if (is_browser_package(description, section, tags)) {
        category = CATEGORY_BROWSER;
    }
    pkg_add(name, version, description, PKG_STATE_NOT_INSTALLED, category,
            is_gnuchan_binary(name), 0, 0);
    Package *package = pkg_find(name);
    if (package) {
        if (version && version[0]) {
            snprintf(package->remote_version, sizeof(package->remote_version),
                     "%s", version);
        }
        if (maintainer && maintainer[0]) {
            snprintf(package->maintainer, sizeof(package->maintainer), "%s",
                     maintainer);
        }
        if (homepage && homepage[0]) {
            snprintf(package->homepage, sizeof(package->homepage), "%s",
                     homepage);
        }
        if (depends && depends[0]) {
            snprintf(package->depends, sizeof(package->depends), "%s",
                     depends);
        }
        if (license && license[0]) {
            snprintf(package->license, sizeof(package->license), "%s",
                     license);
        }
        if (installed_size_kb) package->installed_size_kb = installed_size_kb;
        if (download_size_bytes) {
            package->download_size_bytes = download_size_bytes;
        }
    }
}

static void append_catalog_field(char *destination, size_t capacity,
                                const char *value) {
    size_t used = strlen(destination);
    while (*value == ' ' || *value == '\t') value++;
    size_t length = strcspn(value, "\r\n");
    if (used && used + 1 < capacity) {
        destination[used++] = ' ';
        destination[used] = '\0';
    }
    if (length > capacity - used - 1) {
        length = capacity - used - 1;
    }
    memcpy(destination + used, value, length);
    destination[used + length] = '\0';
}

static void parse_available_catalog(FILE *fp) {
    char line[LINE_BUF_LEN];
    char name[MAX_NAME_LEN] = {0};
    char version[MAX_VERSION_LEN] = {0};
    char section[128] = {0};
    char description[MAX_DESC_LEN] = {0};
    char tags[MAX_TAGS_LEN] = {0};
    char maintainer[MAX_METADATA_LEN] = {0};
    char homepage[MAX_METADATA_LEN] = {0};
    char depends[MAX_DEPENDS_LEN] = {0};
    char license[MAX_METADATA_LEN] = {0};
    unsigned long installed_size_kb = 0;
    unsigned long download_size_bytes = 0;
    enum { FIELD_NONE, FIELD_DESCRIPTION, FIELD_TAGS, FIELD_DEPENDS } field =
        FIELD_NONE;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\r') {
            pthread_mutex_lock(&state_mutex);
            add_catalog_package(name, version, description, section, tags,
                                maintainer, homepage, depends, license,
                                installed_size_kb,
                                download_size_bytes);
            pthread_mutex_unlock(&state_mutex);
            name[0] = version[0] = section[0] = description[0] = '\0';
            tags[0] = '\0';
            maintainer[0] = homepage[0] = '\0';
            depends[0] = '\0';
            license[0] = '\0';
            installed_size_kb = download_size_bytes = 0;
            field = FIELD_NONE;
            continue;
        }
        if (field != FIELD_NONE && isspace((unsigned char)line[0])) {
            char *destination = field == FIELD_DESCRIPTION ? description
                                : field == FIELD_TAGS ? tags : depends;
            size_t capacity = field == FIELD_DESCRIPTION ? sizeof(description)
                              : field == FIELD_TAGS ? sizeof(tags)
                                                    : sizeof(depends);
            append_catalog_field(destination, capacity, line);
            continue;
        }
        field = FIELD_NONE;
        if (strncmp(line, "Package: ", 9) == 0) {
            snprintf(name, sizeof(name), "%.*s",
                     (int)strcspn(line + 9, "\r\n"), line + 9);
        } else if (strncmp(line, "Version: ", 9) == 0) {
            snprintf(version, sizeof(version), "%.*s",
                     (int)strcspn(line + 9, "\r\n"), line + 9);
        } else if (strncmp(line, "Section: ", 9) == 0) {
            snprintf(section, sizeof(section), "%.*s",
                     (int)strcspn(line + 9, "\r\n"), line + 9);
        } else if (strncmp(line, "Tag: ", 5) == 0) {
            snprintf(tags, sizeof(tags), "%.*s",
                     (int)strcspn(line + 5, "\r\n"), line + 5);
            field = FIELD_TAGS;
        } else if (strncmp(line, "Maintainer: ", 12) == 0) {
            snprintf(maintainer, sizeof(maintainer), "%.*s",
                     (int)strcspn(line + 12, "\r\n"), line + 12);
        } else if (strncmp(line, "Homepage: ", 10) == 0) {
            snprintf(homepage, sizeof(homepage), "%.*s",
                     (int)strcspn(line + 10, "\r\n"), line + 10);
        } else if (strncmp(line, "License: ", 9) == 0) {
            snprintf(license, sizeof(license), "%.*s",
                     (int)strcspn(line + 9, "\r\n"), line + 9);
        } else if (strncmp(line, "Depends: ", 9) == 0 ||
                   strncmp(line, "Pre-Depends: ", 13) == 0) {
            size_t offset = line[0] == 'D' ? 9 : 13;
            snprintf(depends, sizeof(depends), "%.*s",
                     (int)strcspn(line + offset, "\r\n"), line + offset);
            field = FIELD_DEPENDS;
        } else if (strncmp(line, "Installed-Size: ", 16) == 0) {
            installed_size_kb = strtoul(line + 16, NULL, 10);
        } else if (strncmp(line, "Size: ", 6) == 0) {
            download_size_bytes = strtoul(line + 6, NULL, 10);
        } else if (strncmp(line, "Description: ", 13) == 0 ||
                   strncmp(line, "Description-", 12) == 0) {
            char *value = NULL;
            if (strncmp(line, "Description: ", 13) == 0) {
                value = line + 13;
            } else {
                char *colon = strchr(line + 12, ':');
                if (colon && isalpha((unsigned char)line[12]) &&
                    strncmp(line + 12, "md5", (size_t)(colon - (line + 12))) != 0) {
                    value = colon + 1;
                }
            }
            if (value) {
                while (*value == ' ' || *value == '\t') value++;
                snprintf(description, sizeof(description), "%.*s",
                         (int)strcspn(value, "\r\n"), value);
                field = FIELD_DESCRIPTION;
            }
        }
    }
    pthread_mutex_lock(&state_mutex);
    add_catalog_package(name, version, description, section, tags, maintainer,
                        homepage, depends, license, installed_size_kb,
                        download_size_bytes);
    pthread_mutex_unlock(&state_mutex);
}

static void load_available_packages(void) {
    FILE *fp = popen("apt-cache dumpavail 2>/dev/null", "r");
    if (!fp) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("Could not start apt-cache",
                               "apt-cache baslatilamadi"));
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    parse_available_catalog(fp);
    int rc = pclose(fp);
    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("apt-cache could not read the repository catalog",
                               "apt-cache depo katalugunu okuyamadi"));
        pthread_mutex_unlock(&state_mutex);
    }
}

static int valid_flatpak_token(const char *token) {
    if (!token || !token[0] || strlen(token) >= MAX_NAME_LEN) return 0;
    for (const unsigned char *p = (const unsigned char *)token; *p; p++) {
        if (!(isalnum(*p) || *p == '.' || *p == '_' || *p == '-')) {
            return 0;
        }
    }
    return 1;
}

static void add_flatpak_package(const char *application, const char *version,
                                const char *remote, PkgState state) {
    if (!valid_flatpak_token(application) ||
        (remote && remote[0] && !valid_flatpak_token(remote))) {
        return;
    }
    pthread_mutex_lock(&state_mutex);
    pkg_add(application, version, "Flatpak application",
            state, CATEGORY_OTHER, 0, 0, 0);
    Package *package = pkg_find(application);
    if (package) {
        package->source = SOURCE_FLATPAK;
        if (remote && remote[0]) {
            snprintf(package->origin, sizeof(package->origin), "%s", remote);
        }
        if (version && version[0]) {
            snprintf(package->remote_version, sizeof(package->remote_version),
                     "%s", version);
        }
    }
    pthread_mutex_unlock(&state_mutex);
}

static int parse_flatpak_applications(FILE *fp, const char *remote,
                                      PkgState state, int with_metadata) {
    char line[LINE_BUF_LEN];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        char application[MAX_NAME_LEN] = {0};
        char version[MAX_VERSION_LEN] = {0};
        char origin[MAX_METADATA_LEN] = {0};
        int fields = with_metadata
            ? sscanf(line, "%255s %63s %159s", application, version, origin)
            : sscanf(line, "%255s", application);
        if (fields < 1 || strcasecmp(application, "application") == 0 ||
            strcasecmp(application, "applicationid") == 0) continue;
        const char *package_remote = with_metadata && origin[0]
                                         ? origin : remote;
        add_flatpak_package(application, with_metadata ? version : "",
                            package_remote, state);
        if (pkg_find(application) &&
            pkg_find(application)->source == SOURCE_FLATPAK) count++;
    }
    return count;
}

static int read_flatpak_remote_list(void) {
    flatpak_remote_count = 0;
    char output[CMD_BUF_LEN * 4];
    int rc = run_cmd("flatpak remotes --columns=name 2>/dev/null",
                     output, sizeof(output));
    if (rc != 0) return rc;
    char *saveptr = NULL;
    for (char *line = strtok_r(output, "\r\n", &saveptr); line;
         line = strtok_r(NULL, "\r\n", &saveptr)) {
        while (isspace((unsigned char)*line)) line++;
        char *end = line + strlen(line);
        while (end > line && isspace((unsigned char)end[-1])) *--end = '\0';
        if (!valid_flatpak_token(line) ||
            strcasecmp(line, "name") == 0) continue;
        if (flatpak_remote_count >= MAX_FLATPAK_REMOTES) break;
        snprintf(flatpak_remotes[flatpak_remote_count++],
                 sizeof(flatpak_remotes[0]), "%s", line);
    }
    return 0;
}

static void load_flatpak_packages(void) {
    char output[CMD_BUF_LEN * 4];
    flatpak_available = run_cmd("command -v flatpak >/dev/null 2>&1", output,
                                sizeof(output)) == 0;
    if (!flatpak_available) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text(
            "Flatpak is unavailable; install flatpak to use this source",
            "Flatpak kullanilamiyor; bu kaynak icin flatpak kurun"));
        state_revision++;
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    read_flatpak_remote_list();
    int installed_rc = run_cmd(
        "flatpak list --app --columns=application,version,origin 2>/dev/null",
        output, sizeof(output));
    if (installed_rc == 0) {
        FILE *installed = fmemopen(output, strlen(output), "r");
        if (installed) {
            parse_flatpak_applications(installed, "", PKG_STATE_INSTALLED, 1);
            fclose(installed);
        }
    }

    for (int i = 0; i < flatpak_remote_count; i++) {
        char command[CMD_BUF_LEN];
        snprintf(command, sizeof(command),
                 "flatpak remote-ls --app --columns=application '%.159s' "
                 "2>/dev/null", flatpak_remotes[i]);
        if (run_cmd(command, output, sizeof(output)) != 0) continue;
        FILE *remote_apps = fmemopen(output, strlen(output), "r");
        if (remote_apps) {
            parse_flatpak_applications(remote_apps, flatpak_remotes[i],
                                       PKG_STATE_NOT_INSTALLED, 0);
            fclose(remote_apps);
        }
    }
    pthread_mutex_lock(&state_mutex);
    snprintf(source_status, sizeof(source_status),
             "Flatpak: %d remote(s)", flatpak_remote_count);
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
}

static int valid_flatpak_url(const char *url) {
    if (!url || !url[0] || strlen(url) >= 1024) return 0;
    for (const unsigned char *p = (const unsigned char *)url; *p; p++) {
        if (!(isalnum(*p) || strchr(":/._-+?&=%@~", *p))) return 0;
    }
    return 1;
}

static int manage_flatpak_remote(int add, const char *name, const char *url) {
    if (!valid_flatpak_token(name) || !flatpak_available) return -1;
    char command[CMD_BUF_LEN];
    if (add) {
        if (!valid_flatpak_url(url)) return -1;
        snprintf(command, sizeof(command),
                 "flatpak remote-add --user --if-not-exists '%s' '%s' 2>&1",
                 name, url);
    } else {
        snprintf(command, sizeof(command),
                 "flatpak remote-delete --user '%s' 2>&1", name);
    }
    char output[CMD_BUF_LEN * 4];
    int rc = run_cmd(command, output, sizeof(output));
    if (rc == 0) {
        printf("%s Flatpak remote %s\n", add ? "Added" : "Removed", name);
    } else {
        fprintf(stderr, "Flatpak remote operation failed: %.300s\n",
                output[0] ? output : "command failed");
    }
    return rc;
}

static void save_source_setting(const char *home) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char path[CONFIG_PATH_LEN], directory[CONFIG_PATH_LEN];
    if (xdg && xdg[0] == '/') {
        if (snprintf(directory, sizeof(directory), "%s/GnuChanSoftwareCenter",
                     xdg) >= (int)sizeof(directory)) return;
    } else {
        if (snprintf(directory, sizeof(directory),
                     "%s/.config/GnuChanSoftwareCenter", home) >=
            (int)sizeof(directory)) return;
    }
    if (mkdir(directory, 0700) != 0 && errno != EEXIST) return;
    if (snprintf(path, sizeof(path), "%s/GnuChanSoftwareCenter.conf",
                 directory) >= (int)sizeof(path)) return;

    char existing[CMD_BUF_LEN] = {0};
    FILE *input = fopen(path, "r");
    if (input) {
        size_t used = 0;
        char line[LINE_BUF_LEN];
        while (fgets(line, sizeof(line), input) &&
               used + strlen(line) < sizeof(existing)) {
            if (strncmp(line, "Source", 6) == 0) continue;
            size_t length = strlen(line);
            memcpy(existing + used, line, length);
            used += length;
            existing[used] = '\0';
        }
        fclose(input);
    }
    FILE *output = fopen(path, "w");
    if (!output) return;
    fputs(existing, output);
    fprintf(output, "Source = \"%s\"\n",
            current_source == SOURCE_FLATPAK ? "flatpak" : "apt");
    fclose(output);
    chmod(path, 0600);
}

static int flatpak_remote_field(const char *remote, const char *application,
                                const char *field, char *out,
                                size_t out_size) {
    if (!valid_flatpak_token(remote) || !valid_flatpak_token(application) ||
        !(strcmp(field, "description") == 0 ||
          strcmp(field, "version") == 0 ||
          strcmp(field, "download-size") == 0)) return -1;
    char command[CMD_BUF_LEN];
    snprintf(command, sizeof(command),
             "flatpak remote-ls --cached --columns=%s:full '%s' '%s' "
             "2>/dev/null", field, remote, application);
    char output[CMD_BUF_LEN * 2];
    int rc = run_cmd(command, output, sizeof(output));
    if (rc != 0) return rc;
    char *value = output;
    while (isspace((unsigned char)*value)) value++;
    if (strncasecmp(value, field, strlen(field)) == 0) {
        char *newline = strpbrk(value, "\r\n");
        if (newline) value = newline + 1;
    }
    while (isspace((unsigned char)*value)) value++;
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) *--end = '\0';
    snprintf(out, out_size, "%s", value);
    return 0;
}

static void *flatpak_details_worker(void *arg) {
    char *identity = arg;
    char application[MAX_NAME_LEN];
    char remote[MAX_METADATA_LEN];
    char *separator = strchr(identity, '\n');
    if (!separator) {
        free(identity);
        return NULL;
    }
    *separator = '\0';
    snprintf(application, sizeof(application), "%s", identity);
    snprintf(remote, sizeof(remote), "%s", separator + 1);
    free(identity);

    char description[MAX_DESC_LEN] = {0};
    char version[MAX_VERSION_LEN] = {0};
    char download_size[64] = {0};
    flatpak_remote_field(remote, application, "description", description,
                         sizeof(description));
    flatpak_remote_field(remote, application, "version", version,
                         sizeof(version));
    flatpak_remote_field(remote, application, "download-size", download_size,
                         sizeof(download_size));

    pthread_mutex_lock(&state_mutex);
    Package *package = pkg_find(application);
    if (current_source == SOURCE_FLATPAK && package &&
        package->source == SOURCE_FLATPAK &&
        strcmp(package->origin, remote) == 0) {
        if (description[0]) {
            snprintf(package->desc, sizeof(package->desc), "%s", description);
        }
        if (version[0]) {
            snprintf(package->remote_version, sizeof(package->remote_version),
                     "%s", version);
            if (!package->version[0]) {
                snprintf(package->version, sizeof(package->version), "%s",
                         version);
            }
        }
        if (download_size[0]) {
            package->download_size_bytes = strtoul(download_size, NULL, 10);
        }
    }
    details_loading = 0;
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
    return NULL;
}

static void request_package_details(Package *package) {
    if (!package || package->source != SOURCE_FLATPAK ||
        !package->origin[0] || details_loading) return;
    char *identity = malloc(MAX_NAME_LEN + MAX_METADATA_LEN + 2);
    if (!identity) return;
    snprintf(identity, MAX_NAME_LEN + MAX_METADATA_LEN + 2, "%s\n%s",
             package->name, package->origin);
    pthread_mutex_lock(&state_mutex);
    details_loading = 1;
    pthread_mutex_unlock(&state_mutex);
    pthread_t thread;
    if (pthread_create(&thread, NULL, flatpak_details_worker, identity) == 0) {
        pthread_detach(thread);
    } else {
        free(identity);
        pthread_mutex_lock(&state_mutex);
        details_loading = 0;
        pthread_mutex_unlock(&state_mutex);
    }
}

static void *source_load_worker(void *arg) {
    const char *home = arg;
    pthread_mutex_lock(&state_mutex);
    pkg_free_all();
    loading_packages = 1;
    update_available = 0;
    state_revision++;
    pthread_mutex_unlock(&state_mutex);

    if (current_source == SOURCE_FLATPAK) {
        load_flatpak_packages();
    } else {
        load_installed_packages();
        add_user_gnuchan_programs(home);
        refresh_update_checks(home);
    }

    pthread_mutex_lock(&state_mutex);
    loading_packages = 0;
    busy = 0;
    selected_index = -1;
    scroll_offset = 0;
    rebuild_visible_list();
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
    return NULL;
}

static void select_package_source(PackageSource source) {
    if (busy || source == current_source) return;
    current_source = source;
    busy = 1;
    save_source_setting(getenv("HOME") ? getenv("HOME") : "/root");
    pthread_t thread;
    int result = pthread_create(&thread, NULL, source_load_worker,
                                (void *)(getenv("HOME") ? getenv("HOME") :
                                                         "/root"));
    if (result == 0) {
        pthread_detach(thread);
        return;
    }
    busy = 0;
    set_status_fmt("Could not switch package source: %s", strerror(result));
}

static void load_installed_packages(void) {
    const char *cmd =
        "dpkg-query -W -f='${Package}\\t${Version}\\t${db:Status-Status}\\t${Section}\\n' 2>/dev/null";
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("Could not start dpkg-query",
                               "dpkg-query baslatilamadi"));
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    char line[LINE_BUF_LEN];
    while (fgets(line, sizeof(line), fp)) {
        char name[MAX_NAME_LEN] = {0};
        char version[MAX_VERSION_LEN] = {0};
        char status[32] = {0};
        char section[128] = {0};
        if (sscanf(line, "%255[^\t]\t%63[^\t]\t%31[^\t]\t%127[^\n]",
                   name, version, status, section) < 3 || !name[0]) {
            continue;
        }

        int installed = strcmp(status, "installed") == 0;
        int is_gc = is_gnuchan_binary(name);
        pthread_mutex_lock(&state_mutex);
        pkg_add(name, version, "",
                installed ? PKG_STATE_INSTALLED : PKG_STATE_NOT_INSTALLED,
                category_for_section(section), is_gc, 0, 0);
        pthread_mutex_unlock(&state_mutex);
    }

    int rc = pclose(fp);
    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("dpkg-query failed", "dpkg-query basarisiz"));
        pthread_mutex_unlock(&state_mutex);
    }
    load_available_packages();
}

/* Guncelleme denetimi: apt-get -s upgrade */
static Package *find_apt_package(const char *name) {
    Package *p = pkg_find(name);
    if (p) return p;
    const char *architecture = strchr(name, ':');
    if (!architecture) return NULL;
    char base_name[MAX_NAME_LEN];
    size_t length = (size_t)(architecture - name);
    if (length == 0 || length >= sizeof(base_name)) return NULL;
    memcpy(base_name, name, length);
    base_name[length] = '\0';
    return pkg_find(base_name);
}

static void mark_apt_updates(FILE *fp) {
    char line[LINE_BUF_LEN];
    int count = 0;
    pthread_mutex_lock(&state_mutex);
    while (fgets(line, sizeof(line), fp)) {
        char name[MAX_NAME_LEN] = {0};
        if (sscanf(line, "Inst %255s", name) != 1) continue;
        Package *p = find_apt_package(name);
        if (p && !p->apt_update) {
            p->apt_update = 1;
            p->has_update = 1;
            count++;
        }
    }
    update_available = count;
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
}

static void check_apt_updates(void) {
    FILE *fp = popen("apt-get -s upgrade 2>/dev/null", "r");
    if (!fp) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("Could not check apt updates",
                               "apt guncelleme denetimi calismadi"));
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    mark_apt_updates(fp);
    int rc = pclose(fp);
    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
        pthread_mutex_lock(&state_mutex);
        set_status(center_text("Could not check apt updates",
                               "apt guncelleme denetimi calismadi"));
        pthread_mutex_unlock(&state_mutex);
    }
}

static int repo_config_exists(const char *home, const GnuchanRepo *repo) {
    char path[CONFIG_PATH_LEN];
    if (repo_config_path(home, repo, path, sizeof(path)) &&
        access(path, R_OK) == 0) {
        return 1;
    }

    char executable[CONFIG_PATH_LEN];
    const char *fixed_dirs[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    for (size_t i = 0; fixed_dirs[i]; i++) {
        snprintf(executable, sizeof(executable), "%s/%s",
                 fixed_dirs[i], repo->program);
        if (access(executable, X_OK) == 0) return 1;
    }

    const char *path_env = getenv("PATH");
    while (path_env && *path_env) {
        const char *end = strchr(path_env, ':');
        size_t directory_length = end ? (size_t)(end - path_env)
                                      : strlen(path_env);
        if (directory_length > 0 &&
            directory_length + strlen(repo->program) + 2 <
                sizeof(executable)) {
            memcpy(executable, path_env, directory_length);
            executable[directory_length] = '/';
            snprintf(executable + directory_length + 1,
                     sizeof(executable) - directory_length - 1, "%s",
                     repo->program);
            if (access(executable, X_OK) == 0) return 1;
        }
        if (!end) break;
        path_env = end + 1;
    }
    return 0;
}

static void add_user_gnuchan_programs(const char *home) {
    pthread_mutex_lock(&state_mutex);
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        GnuchanRepo *repo = &gnuchan_repos[i];
        if (!repo_config_exists(home, repo)) continue;
        read_config_version(home, repo, repo->local_version,
                            sizeof(repo->local_version));
        Package *pkg = pkg_find(repo->program);
        if (pkg) {
            pkg->state = PKG_STATE_INSTALLED;
            pkg->is_gnuchan = 1;
            snprintf(pkg->version, sizeof(pkg->version), "%s",
                     repo->local_version);
        } else {
            pkg_add(repo->program, repo->local_version, "GnuChan program",
                    PKG_STATE_INSTALLED, CATEGORY_OTHER, 1, 0, 0);
        }
    }
    pthread_mutex_unlock(&state_mutex);
}

/* Check GitHub versions for the GnuChan programs installed for this user. */
static void check_github_versions(const char *home) {
    int failures = 0;
    int checked = 0;
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        GnuchanRepo *r = &gnuchan_repos[i];
        r->has_remote = 0;
        r->update_available = 0;
        r->remote_version[0] = '\0';

        /* Only check programs installed for this user. */
        if (!repo_config_exists(home, r)) {
            snprintf(r->local_version, sizeof(r->local_version), "0.0.0");
            continue;
        }
        checked++;

        read_config_version(home, r, r->local_version,
                            sizeof(r->local_version));

        /* Uzak versiyonu GitHub'dan al */
        char url[512];
        snprintf(url, sizeof(url),
                 "https://raw.githubusercontent.com/gnuchanos/%s/main/version.txt", r->repo);
        char cmd[CMD_BUF_LEN];
        snprintf(cmd, sizeof(cmd),
                 "curl -fsS --connect-timeout 3 --max-time 8 '%s' 2>/dev/null",
                 url);
        char out[CMD_BUF_LEN];
        if (run_cmd(cmd, out, sizeof(out)) == 0 && out[0]) {
            /* Bosluklari temizle */
            char *s = out;
            while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
            char *e = s + strlen(s);
            if (e > s) {
                e--;
            }
            while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) *e-- = '\0';
            if (version_valid(s) && version_valid(r->local_version)) {
                memcpy(r->remote_version, s, strlen(s) + 1);
                r->has_remote = 1;
                if (version_newer(r->remote_version, r->local_version)) {
                    r->update_available = 1;
                }
            } else {
                failures++;
            }
        } else {
            failures++;
        }
    }

    pthread_mutex_lock(&state_mutex);
    github_check_failures = failures;
    gnuchan_update_count = 0;
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        if (gnuchan_repos[i].update_available) gnuchan_update_count++;
    }
    github_check_done = 1;
    if (github_check_failures > 0) {
        set_status_fmt(center_text(
                           "GitHub check incomplete: %d of %d installed GnuChan program(s) unavailable",
                           "GitHub denetimi tamamlanamadi: %d kurulu GnuChan programindan %d tanesine ulasilamadi"),
                       github_check_failures,
                       checked);
    } else {
        set_status_fmt(center_text(
                           "GitHub checked %d installed GnuChan program(s); %d update(s)",
                           "GitHub %d kurulu GnuChan programini denetledi; %d guncelleme bulundu"),
                       checked,
                       gnuchan_update_count);
    }

    /* Include user-installed GnuChan programs even when they are not dpkg packages. */
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        if (!repo_config_exists(home, &gnuchan_repos[i])) continue;
        Package *p = pkg_find(gnuchan_repos[i].program);
        if (p) {
            p->state = PKG_STATE_INSTALLED;
            p->is_gnuchan = 1;
            snprintf(p->version, sizeof(p->version), "%s",
                     gnuchan_repos[i].local_version);
            snprintf(p->remote_version, sizeof(p->remote_version), "%s",
                     gnuchan_repos[i].remote_version);
            p->github_update = gnuchan_repos[i].update_available;
            if (gnuchan_repos[i].update_available) {
                p->has_update = 1;
            }
        } else {
            pkg_add(gnuchan_repos[i].program,
                    gnuchan_repos[i].local_version,
                    "GnuChan program", PKG_STATE_INSTALLED, CATEGORY_OTHER,
                    1, gnuchan_repos[i].update_available,
                    gnuchan_repos[i].update_available);
            p = pkg_find(gnuchan_repos[i].program);
            if (p) {
                snprintf(p->remote_version, sizeof(p->remote_version), "%s",
                         gnuchan_repos[i].remote_version);
            }
        }
    }
    checking_updates = 0;
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
}

/* --- paket islemleri ----------------------------------------------------- */

static void record_action(const char *action, const char *package, int success,
                          int exit_code) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/root";
    const char *xdg_state = getenv("XDG_STATE_HOME");
    char state_dir[CONFIG_PATH_LEN];
    if (xdg_state && xdg_state[0] == '/') {
        snprintf(state_dir, sizeof(state_dir), "%s", xdg_state);
    } else {
        char local_dir[CONFIG_PATH_LEN];
        if (snprintf(local_dir, sizeof(local_dir), "%s/.local", home) >=
            (int)sizeof(local_dir)) return;
        if (mkdir(local_dir, 0700) != 0 && errno != EEXIST) return;
        size_t local_length = strlen(local_dir);
        if (local_length + sizeof("/state") > sizeof(state_dir)) return;
        memcpy(state_dir, local_dir, local_length);
        memcpy(state_dir + local_length, "/state", sizeof("/state"));
    }
    if (mkdir(state_dir, 0700) != 0 && errno != EEXIST) return;

    char history_dir[CONFIG_PATH_LEN];
    if (snprintf(history_dir, sizeof(history_dir),
                 "%s/GnuChanSoftwareCenter", state_dir) >=
        (int)sizeof(history_dir)) return;
    if (mkdir(history_dir, 0700) != 0 && errno != EEXIST) return;

    char history_path[CONFIG_PATH_LEN];
    if (snprintf(history_path, sizeof(history_path), "%s/history.log",
                 history_dir) >= (int)sizeof(history_path)) return;
    FILE *history = fopen(history_path, "a");
    if (!history) return;
    fchmod(fileno(history), 0600);
    struct timespec now;
    struct tm local_time;
    char timestamp[32] = "unknown-time";
    if (clock_gettime(CLOCK_REALTIME, &now) == 0 &&
        localtime_r(&now.tv_sec, &local_time)) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                 &local_time);
    }
    fprintf(history, "%s\t%s\t%s\t%s\texit=%d\n", timestamp, action,
            package && package[0] ? package : "-", success ? "success" : "failed",
            exit_code);
    fclose(history);
}

static int valid_package_name(const char *name) {
    if (!name || !(islower((unsigned char)name[0]) ||
                   isdigit((unsigned char)name[0]))) {
        return 0;
    }
    int architecture_separator_seen = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (*p == ':') {
            if (architecture_separator_seen || p == (const unsigned char *)name ||
                p[1] == '\0') {
                return 0;
            }
            architecture_separator_seen = 1;
            continue;
        }
        if (!(islower(*p) || isdigit(*p) || *p == '+' || *p == '.' ||
              *p == '-')) {
            return 0;
        }
    }
    return 1;
}

static int do_install(const char *name) {
    if (!valid_package_name(name)) {
        set_status(center_text("Refused an invalid package name",
                               "Gecersiz paket adi reddedildi"));
        return -1;
    }
    char cmd[CMD_BUF_LEN];
    const char *auth = require_authentication && getuid() != 0
                           ? "pkexec " : "";
    snprintf(cmd, sizeof(cmd),
             "%sapt-get -o APT::Status-Fd=1 -o Dpkg::Use-Pty=0 install "
             "-y --no-install-recommends --no-remove '%s' 2>&1",
             auth, name);
    char out[CMD_BUF_LEN * 4];
    int rc = run_cmd(cmd, out, sizeof(out));
    if (rc == 0) {
        record_action("install", name, 1, rc);
        set_status_fmt(center_text("%s installed", "%s kuruldu"), name);
        return 0;
    }
    record_action("install", name, 0, rc);
    set_status_fmt(center_text("%s could not be installed: %.200s",
                               "%s kurulamadi: %.200s"), name, out);
    return -1;
}

static int do_upgrade_package(const char *name) {
    if (!valid_package_name(name)) {
        set_status(center_text("Refused an invalid package name",
                               "Gecersiz paket adi reddedildi"));
        return -1;
    }
    char cmd[CMD_BUF_LEN];
    const char *auth = require_authentication && getuid() != 0
                           ? "pkexec " : "";
    snprintf(cmd, sizeof(cmd),
             "%sapt-get -o APT::Status-Fd=1 -o Dpkg::Use-Pty=0 install "
             "--only-upgrade -y --no-install-recommends '%s' 2>&1",
             auth, name);
    char out[CMD_BUF_LEN * 4];
    int rc = run_cmd(cmd, out, sizeof(out));
    record_action("upgrade", name, rc == 0, rc);
    if (rc == 0) {
        set_status_fmt(center_text("%s updated", "%s guncellendi"), name);
        return 0;
    }
    set_status_fmt(center_text("%s could not be updated: %.200s",
                               "%s guncellenemedi: %.200s"), name, out);
    return -1;
}

static int do_remove(const char *name) {
    if (!valid_package_name(name)) {
        set_status(center_text("Refused an invalid package name",
                               "Gecersiz paket adi reddedildi"));
        return -1;
    }
    char cmd[CMD_BUF_LEN];
    const char *auth = require_authentication && getuid() != 0
                           ? "pkexec " : "";
    snprintf(cmd, sizeof(cmd),
             "%sapt-get -o APT::Status-Fd=1 -o Dpkg::Use-Pty=0 remove "
             "-y '%s' 2>&1",
             auth, name);
    char out[CMD_BUF_LEN * 4];
    int rc = run_cmd(cmd, out, sizeof(out));
    if (rc == 0) {
        record_action("remove", name, 1, rc);
        set_status_fmt(center_text("%s removed", "%s kaldirildi"), name);
        return 0;
    }
    record_action("remove", name, 0, rc);
    set_status_fmt(center_text("%s could not be removed: %.200s",
                               "%s kaldirilamadi: %.200s"), name, out);
    return -1;
}

static int do_upgrade_all(void) {
    char cmd[CMD_BUF_LEN];
    const char *auth = require_authentication && getuid() != 0
                           ? "pkexec " : "";
    snprintf(cmd, sizeof(cmd),
             "%s/bin/sh -c 'apt-get -o APT::Status-Fd=1 "
             "-o Dpkg::Use-Pty=0 update -o Acquire::Retries=2 && "
             "apt-get -o APT::Status-Fd=1 -o Dpkg::Use-Pty=0 upgrade -y' "
             "2>&1", auth);
    char out[CMD_BUF_LEN * 4];
    int rc = run_cmd(cmd, out, sizeof(out));
    record_action("upgrade-all", NULL, rc == 0, rc);
    if (rc == 0) {
        set_status(center_text("All updates installed",
                               "tum guncellemeler uygulandi"));
        return 0;
    }
    set_status_fmt(center_text("Update failed: %.200s",
                               "guncelleme basarisiz: %.200s"), out);
    return -1;
}

static int do_update_gnuchan_all(void) {
    const struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_name || !account->pw_name[0]) {
        set_status(center_text(
            "GnuChan update failed: cannot identify the current account",
            "GnuChan guncellemesi basarisiz: kullanici hesabi belirlenemedi"));
        return -1;
    }
    char cmd[CMD_BUF_LEN];
    for (const char *p = account->pw_name; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' ||
              *p == '.')) {
            set_status(center_text(
                "GnuChan update failed: invalid current account name",
                "GnuChan guncellemesi basarisiz: gecersiz kullanici adi"));
            return -1;
        }
    }
    const char *auth = getuid() == 0 || !require_authentication
                           ? "" : "pkexec ";
    int written = snprintf(cmd, sizeof(cmd),
                           "%s/usr/local/libexec/gnuchanos-programs-update "
                           "--update '%s' 2>&1", auth, account->pw_name);
    if (written < 0 || (size_t)written >= sizeof(cmd)) {
        set_status(center_text("GnuChan update command is too long",
                               "GnuChan guncelleme komutu cok uzun"));
        return -1;
    }
    size_t pos = (size_t)written;
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        Package *p = pkg_find(gnuchan_repos[i].program);
        if (p && p->state == PKG_STATE_INSTALLED && p->github_update) {
            int added = snprintf(cmd + pos, sizeof(cmd) - pos, " '%s'",
                                 gnuchan_repos[i].program);
            if (added < 0 || (size_t)added >= sizeof(cmd) - pos) {
                set_status(center_text("GnuChan update command is too long",
                                       "GnuChan guncelleme komutu cok uzun"));
                return -1;
            }
            pos += (size_t)added;
        }
    }
    if (pos == (size_t)written) {
        set_status(center_text("No GnuChan programs to update",
                               "guncellenecek GnuChan paketi yok"));
        return 0;
    }

    char out[CMD_BUF_LEN * 4];
    int rc = run_cmd(cmd, out, sizeof(out));
    if (rc == 0) {
        set_status(center_text("GnuChan program updates installed successfully",
                               "GnuChan program guncellemeleri yuklendi"));
        return 0;
    }
    set_status_fmt(center_text("GnuChan update failed (exit %d): %.180s",
                               "GnuChan guncellemesi basarisiz (cikis %d): %.180s"),
                   rc, out);
    return -1;
}

static int do_flatpak_package_action(QueueAction action, const char *name,
                                     const char *remote) {
    if (!flatpak_available ||
        (action != QUEUE_UPDATE_ALL && !valid_flatpak_token(name)) ||
        (remote && remote[0] && !valid_flatpak_token(remote))) {
        set_status(center_text("Flatpak is unavailable or the application ID is invalid",
                               "Flatpak kullanilamiyor veya uygulama kimligi gecersiz"));
        return -1;
    }

    char command[CMD_BUF_LEN];
    if (action == QUEUE_INSTALL) {
        if (!remote || !remote[0]) {
            set_status(center_text("Choose an enabled Flatpak remote first",
                                   "Once etkin bir Flatpak deposu secin"));
            return -1;
        }
        snprintf(command, sizeof(command),
                 "flatpak install --user --noninteractive -y '%s' '%s' 2>&1",
                 remote, name);
    } else if (action == QUEUE_REMOVE) {
        snprintf(command, sizeof(command),
                 "flatpak uninstall --user --noninteractive -y '%s' 2>&1",
                 name);
    } else if (action == QUEUE_UPDATE) {
        snprintf(command, sizeof(command),
                 "flatpak update --user --noninteractive -y '%s' 2>&1", name);
    } else if (action == QUEUE_UPDATE_ALL) {
        snprintf(command, sizeof(command),
                 "flatpak update --user --noninteractive -y 2>&1");
    } else {
        return -1;
    }

    char output[CMD_BUF_LEN * 4];
    int rc = run_cmd(command, output, sizeof(output));
    record_action(action == QUEUE_INSTALL ? "flatpak-install" :
                  action == QUEUE_REMOVE ? "flatpak-remove" :
                  action == QUEUE_UPDATE_ALL ? "flatpak-update-all" :
                                           "flatpak-update",
                  name && name[0] ? name : NULL, rc == 0, rc);
    if (rc != 0) {
        set_status_fmt("Flatpak operation failed: %.300s",
                       output[0] ? output : "command failed");
    }
    return rc;
}

static void execute_queued_action(QueueAction action, PackageSource source,
                                  const char *name, const char *remote) {
    if (source == SOURCE_FLATPAK) {
        do_flatpak_package_action(action, name, remote);
        return;
    }
    switch (action) {
    case QUEUE_INSTALL: do_install(name); break;
    case QUEUE_REMOVE: do_remove(name); break;
    case QUEUE_UPDATE: do_upgrade_package(name); break;
    case QUEUE_UPDATE_ALL: do_upgrade_all(); break;
    case QUEUE_GNUCHAN_UPDATE: do_update_gnuchan_all(); break;
    }
}

static void *queue_worker(void *arg) {
    const char *home = arg;
    for (;;) {
        QueueAction action;
        PackageSource source;
        char name[MAX_NAME_LEN];
        char remote[MAX_METADATA_LEN];
        pthread_mutex_lock(&state_mutex);
        if (queue_count == 0) {
            progress_percent = -1;
            snprintf(progress_status, sizeof(progress_status), "%s",
                     center_text("Refreshing package state...",
                                 "Paket durumu yenileniyor..."));
            set_status(center_text("Queue complete; refreshing package state",
                                   "Kuyruk tamamlandi; paket durumu yenileniyor"));
            state_revision++;
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        int index = queue_head;
        queue_head = (queue_head + 1) % MAX_QUEUE_ITEMS;
        queue_count--;
        action = operation_queue[index];
        source = operation_queue_source[index];
        snprintf(name, sizeof(name), "%s", operation_queue_package[index]);
        snprintf(remote, sizeof(remote), "%s", operation_queue_remote[index]);
        progress_percent = -1;
        progress_status[0] = '\0';
        set_status_fmt(center_text("Queue: %d item(s) remaining",
                                   "Kuyruk: %d oge kaldi"), queue_count);
        state_revision++;
        pthread_mutex_unlock(&state_mutex);

        execute_queued_action(action, source, name, remote);
        pthread_mutex_lock(&state_mutex);
        queue_completed++;
        state_revision++;
        pthread_mutex_unlock(&state_mutex);
    }

    pthread_mutex_lock(&state_mutex);
    pkg_free_all();
    if (current_source == SOURCE_FLATPAK) {
        loading_packages = 0;
        pthread_mutex_unlock(&state_mutex);
        load_flatpak_packages();
    } else {
        loading_packages = 1;
        pthread_mutex_unlock(&state_mutex);
        load_installed_packages();
        add_user_gnuchan_programs(home);
        refresh_update_checks(home);
        pthread_mutex_lock(&state_mutex);
        loading_packages = 0;
        pthread_mutex_unlock(&state_mutex);
    }
    pthread_mutex_lock(&state_mutex);
    rebuild_visible_list();
    queue_worker_running = 0;
    busy = 0;
    progress_percent = -1;
    progress_status[0] = '\0';
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
    return NULL;
}

static int enqueue_operation(QueueAction action, PackageSource source,
                             const char *name, const char *remote) {
    pthread_mutex_lock(&state_mutex);
    if (queue_count >= MAX_QUEUE_ITEMS) {
        set_status(center_text("Operation queue is full",
                               "Islem kuyrugu dolu"));
        pthread_mutex_unlock(&state_mutex);
        return 0;
    }
    int index = (queue_head + queue_count) % MAX_QUEUE_ITEMS;
    operation_queue[index] = action;
    operation_queue_source[index] = source;
    snprintf(operation_queue_package[index],
             sizeof(operation_queue_package[index]), "%s", name ? name : "");
    snprintf(operation_queue_remote[index],
             sizeof(operation_queue_remote[index]), "%s", remote ? remote : "");
    queue_count++;
    queue_total++;
    set_status_fmt(center_text("Added to queue (%d pending)",
                               "Kuyruga eklendi (%d bekliyor)"), queue_count);
    state_revision++;
    pthread_mutex_unlock(&state_mutex);
    return 1;
}

static void start_queue(void) {
    pthread_mutex_lock(&state_mutex);
    if (queue_count == 0 || queue_worker_running) {
        pthread_mutex_unlock(&state_mutex);
        return;
    }
    queue_worker_running = 1;
    busy = 1;
    queue_completed = 0;
    state_revision++;
    pthread_t thread;
    int result = pthread_create(&thread, NULL, queue_worker,
                                (void *)(getenv("HOME") ? getenv("HOME") :
                                                         "/root"));
    if (result == 0) {
        pthread_detach(thread);
        pthread_mutex_unlock(&state_mutex);
        return;
    }
    queue_worker_running = 0;
    busy = 0;
    set_status_fmt("Could not start operation queue: %s", strerror(result));
    pthread_mutex_unlock(&state_mutex);
}

/* --- UI cizim yardimcilari --------------------------------------------- */

static void fill_rect(Display *dpy, Window win, GC gc, unsigned long color,
                      int x, int y, unsigned int w, unsigned int h) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

static void draw_rect(Display *dpy, Window win, GC gc, unsigned long color,
                      int x, int y, unsigned int w, unsigned int h) {
    XSetForeground(dpy, gc, color);
    XDrawRectangle(dpy, win, gc, x, y, w, h);
}

static void draw_text(Display *dpy, Window win, GC gc, unsigned long color,
                      int x, int y, const char *text) {
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, win, gc, x, y, text, (int)strlen(text));
}

static unsigned long get_color(Display *dpy, unsigned long rgb) {
    XColor c;
    c.red = (rgb >> 16) & 0xFF;
    c.red = c.red * 256 | c.red;
    c.green = (rgb >> 8) & 0xFF;
    c.green = c.green * 256 | c.green;
    c.blue = rgb & 0xFF;
    c.blue = c.blue * 256 | c.blue;
    c.flags = DoRed | DoGreen | DoBlue;
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    if (!XAllocColor(dpy, cmap, &c)) {
        /* Fallback: if we can't allocate, use the closest pixel directly. */
        return rgb;
    }
    return c.pixel;
}

static GC create_gc(Display *dpy, Window win) {
    GC gc = XCreateGC(dpy, win, 0, NULL);
    Font f = XLoadFont(dpy, "fixed");
    if (!f) {
        f = XLoadFont(dpy, "6x13");
    }
    if (!f) {
        f = XLoadFont(dpy, "cursor");
    }
    XSetFont(dpy, gc, f);
    return gc;
}

/* --- sekme / buton durumu ------------------------------------------------ */

typedef struct SidebarItem {
    const char *label_en;
    const char *label_tr;
    ViewId view;
} SidebarItem;

static SidebarItem sidebar_items[] = {
    {"All packages", "Tum Paketler", VIEW_ALL},
    {"Installed", "Yuklenmisler", VIEW_INSTALLED},
    {"Updates", "Guncellemeler", VIEW_UPDATES},
    {"GnuChan programs", "GnuChan Programlari", VIEW_GNUCHAN},
    {"System", "System", VIEW_SYSTEM},
    {"Internet", "Internet", VIEW_INTERNET},
    {"Graphics", "Graphics", VIEW_GRAPHICS},
    {"Multimedia", "Multimedia", VIEW_MULTIMEDIA},
    {"Development", "Development", VIEW_DEVELOPMENT},
    {"Education", "Egitim", VIEW_EDUCATION},
    {"Science", "Bilim", VIEW_SCIENCE},
    {"Games", "Oyunlar", VIEW_GAMES},
    {"Browser", "Tarayicilar", VIEW_BROWSER},
    {"Fonts", "Yazi Tipleri", VIEW_FONTS},
};
static int sidebar_count =
    (int)(sizeof(sidebar_items) / sizeof(sidebar_items[0]));

/* --- filtreleme / gorunur liste ------------------------------------------ */

typedef struct VisibleEntry {
    Package *pkg;
    int index;
} VisibleEntry;

static VisibleEntry visible_list[MAX_PACKAGES];
static int visible_len = 0;

static int pkg_matches_search(const Package *p, const char *search) {
    if (!search || !search[0]) return 1;
    char haystack[MAX_NAME_LEN + 2 * MAX_VERSION_LEN + MAX_DESC_LEN +
                  3 * MAX_METADATA_LEN + MAX_DEPENDS_LEN + 7];
    snprintf(haystack, sizeof(haystack), "%s %s %s %s %s %s %s %s",
             p->name, p->version, p->remote_version, p->desc,
             p->maintainer, p->homepage, p->license, p->depends);
    for (char *c = haystack; *c; c++) *c = (char)tolower((unsigned char)*c);
    char needle[256];
    snprintf(needle, sizeof(needle), "%s", search);
    for (char *c = needle; *c; c++) *c = (char)tolower((unsigned char)*c);
    return strstr(haystack, needle) != NULL;
}

static int view_matches(ViewId view, const Package *p) {
    switch (view) {
    case VIEW_ALL: return 1;
    case VIEW_INSTALLED: return p->state == PKG_STATE_INSTALLED;
    case VIEW_UPDATES: return p->has_update;
    case VIEW_GNUCHAN: return p->is_gnuchan;
    case VIEW_SYSTEM: return p->category == CATEGORY_SYSTEM;
    case VIEW_INTERNET: return p->category == CATEGORY_INTERNET;
    case VIEW_GRAPHICS: return p->category == CATEGORY_GRAPHICS;
    case VIEW_MULTIMEDIA: return p->category == CATEGORY_MULTIMEDIA;
    case VIEW_DEVELOPMENT: return p->category == CATEGORY_DEVELOPMENT;
    case VIEW_EDUCATION: return p->category == CATEGORY_EDUCATION;
    case VIEW_SCIENCE: return p->category == CATEGORY_SCIENCE;
    case VIEW_GAMES: return p->category == CATEGORY_GAMES;
    case VIEW_BROWSER: return p->category == CATEGORY_BROWSER;
    case VIEW_FONTS: return p->category == CATEGORY_FONTS;
    }
    return 0;
}

static int list_visible_rows(void) {
    int rows = (LIST_BOTTOM - LIST_TOP) / ITEM_H + 1;
    return rows < 1 ? 1 : rows;
}

static int list_max_scroll(void) {
    int max_scroll = visible_len - list_visible_rows();
    return max_scroll > 0 ? max_scroll : 0;
}

static void rebuild_visible_list(void) {
    visible_len = 0;
    for (Package *p = pkg_head; p; p = p->next) {
        if (!view_matches(current_view, p)) continue;
        if (!pkg_matches_search(p, search_text)) continue;
        visible_list[visible_len].pkg = p;
        visible_list[visible_len].index = visible_len;
        visible_len++;
    }
    if (selected_index >= visible_len) selected_index = visible_len - 1;
    if (selected_index < 0 && visible_len > 0) selected_index = 0;

    int item_visible = list_visible_rows();
    int max_scroll = list_max_scroll();
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    if (scroll_offset < 0) scroll_offset = 0;
    if (selected_index >= 0 && selected_index < visible_len) {
        if (selected_index < scroll_offset) {
            scroll_offset = selected_index;
        } else if (selected_index >= scroll_offset + item_visible) {
            scroll_offset = selected_index - item_visible + 1;
        }
    }
}

/* --- cizim --------------------------------------------------------------- */

static void draw_sidebar(Display *dpy, Window win, GC gc) {
    fill_rect(dpy, win, gc, get_color(dpy, COL_SIDEBAR), 0, 0,
              (unsigned int)SIDEBAR_W, (unsigned int)WIN_HEIGHT);
    draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER),
              (unsigned int)SIDEBAR_W, 0, 1, (unsigned int)WIN_HEIGHT);

    int yy = HEADER_H + 20;
    for (int i = 0; i < sidebar_count; i++) {
        int sel = (current_view == sidebar_items[i].view);
        int item_h = 30;
        if (sel) {
            fill_rect(dpy, win, gc, get_color(dpy, COL_SELECT),
                      4, yy - 4, (unsigned int)(SIDEBAR_W - 8), (unsigned int)item_h);
        }
        draw_text(dpy, win, gc,
                  get_color(dpy, sel ? 0xFFFFFFU : COL_TEXT_DIM),
                  14, yy + 14,
                  center_text(sidebar_items[i].label_en,
                              sidebar_items[i].label_tr));

        /* hint */
        char hint[64] = {0};
        if (sidebar_items[i].view == VIEW_UPDATES && update_available) {
            snprintf(hint, sizeof(hint), "(%d)", update_available > 99 ? 99 : update_available);
        } else if (sidebar_items[i].view == VIEW_GNUCHAN && gnuchan_update_count > 0) {
            snprintf(hint, sizeof(hint), "(%d)", gnuchan_update_count);
        }
        if (hint[0]) {
            draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                      SIDEBAR_W - 10 - (int)(strlen(hint) * 6), yy + 14, hint);
        }
        yy += item_h;
    }

    char ver[64];
    snprintf(ver, sizeof(ver), "v%s", GNUCHANSOFTWARECENTER_VERSION);
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
              14, WIN_HEIGHT - 30, ver);
}

static void draw_header(Display *dpy, Window win, GC gc) {
    fill_rect(dpy, win, gc, get_color(dpy, COL_HEADER), 0, 0,
              (unsigned int)WIN_WIDTH, (unsigned int)HEADER_H);
    draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER),
              0, (unsigned int)HEADER_H, (unsigned int)WIN_WIDTH, 1);

    const char *title = "GnuChanSoftwareCenter";
    for (int i = 0; i < sidebar_count; i++) {
        if (current_view == sidebar_items[i].view) {
            static char view_title[96];
            snprintf(view_title, sizeof(view_title),
                     "GnuChanSoftwareCenter - %s",
                     center_text(sidebar_items[i].label_en,
                                 sidebar_items[i].label_tr));
            title = view_title;
            break;
        }
    }
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT), 14, HEADER_H / 2 + 6, title);

    int source_x = source_selector_x();
    fill_rect(dpy, win, gc, get_color(dpy, COL_BG), source_x, 10, 112, 24);
    draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER), source_x, 10, 112, 24);
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT), source_x + 6, 28,
              current_source == SOURCE_FLATPAK ? "Source: Flatpak" :
                                                "Source: APT");

    int sx = WIN_WIDTH - 300;
    fill_rect(dpy, win, gc, get_color(dpy, COL_BG), sx, 10, 280, 24);
    draw_rect(dpy, win, gc,
              get_color(dpy, search_focused ? COL_BUTTON : COL_BORDER),
              sx, 10, 280, 24);
    char visible_search[45];
    size_t visible_search_length = strlen(search_text);
    if (visible_search_length >= sizeof(visible_search)) {
        visible_search_length = sizeof(visible_search) - 1;
    }
    memcpy(visible_search, search_text, visible_search_length);
    visible_search[visible_search_length] = '\0';
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT), sx + 6, 10 + 18,
              visible_search);
    if (search_text[0] == '\0' && !search_focused) {
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), sx + 6, 10 + 18,
                  center_text("Search...", "ara..."));
    }
    if (search_focused) {
        int cursor_x = sx + 6 + (int)(strlen(visible_search) * 6);
        fill_rect(dpy, win, gc, get_color(dpy, COL_TEXT),
                  cursor_x, 15, 1, 14);
    }
}

static void draw_package_list(Display *dpy, Window win, GC gc) {
    if (loading_packages) {
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  LIST_LEFT, LIST_TOP + 20,
                  center_text("Scanning the Debian package catalog...",
                              "Debian paket katalogu taraniyor..."));
        return;
    }
    int yy = LIST_TOP - scroll_offset * ITEM_H;

    for (int i = 0; i < visible_len; i++) {
        Package *p = visible_list[i].pkg;
        if (yy + ITEM_H < LIST_TOP) { yy += ITEM_H; continue; }
        if (yy > LIST_BOTTOM) break;

        int sel = (i == selected_index);
        if (sel) {
            fill_rect(dpy, win, gc, get_color(dpy, COL_SELECT),
                      (unsigned int)LIST_LEFT, (unsigned int)yy,
                      (unsigned int)(LIST_RIGHT - LIST_LEFT), (unsigned int)ITEM_H);
        } else if (i % 2 == 1) {
            fill_rect(dpy, win, gc, get_color(dpy, 0x20112EU),
                      (unsigned int)LIST_LEFT, (unsigned int)yy,
                      (unsigned int)(LIST_RIGHT - LIST_LEFT), (unsigned int)ITEM_H);
        }

        /* durum noktasi */
        unsigned long dot_col = get_color(dpy, COL_TEXT_DIM);
        if (p->has_update) dot_col = get_color(dpy, COL_UPDATE);
        else if (p->state == PKG_STATE_INSTALLED) {
            dot_col = get_color(dpy, COL_SUCCESS);
        }
        fill_rect(dpy, win, gc, dot_col, LIST_LEFT + 2, yy + 9, 6, 6);

        /* gnuchan isareti */
        if (p->is_gnuchan) {
            draw_text(dpy, win, gc, get_color(dpy, COL_BUTTON),
                      LIST_LEFT + 12, yy + 18, "[G]");
        }

        /* paket adi ve yerel versiyon */
        char label[512];
        if (p->has_update && p->remote_version[0] &&
            strcmp(p->remote_version, p->version) != 0) {
            snprintf(label, sizeof(label), "%s  %s -> %s", p->name, p->version, p->remote_version);
            draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                      LIST_LEFT + (p->is_gnuchan ? 40 : 18), yy + 18, label);
            /* guncelleme var isareti */
            draw_text(dpy, win, gc, get_color(dpy, COL_UPDATE),
                      LIST_RIGHT - 145, yy + 18,
                      center_text("UPDATE", "GUNCELLEME"));
        } else {
            snprintf(label, sizeof(label), "%s  %s", p->name, p->version);
            draw_text(dpy, win, gc, get_color(dpy, COL_TEXT),
                      LIST_LEFT + (p->is_gnuchan ? 40 : 18), yy + 18, label);
        }

        const char *state_label = p->has_update
            ? center_text("Update", "Guncelle")
            : p->state == PKG_STATE_INSTALLED
                  ? center_text("Installed", "Yuklu")
                  : center_text("Available", "Mevcut");
        draw_text(dpy, win, gc,
                  get_color(dpy, p->has_update ? COL_UPDATE
                               : p->state == PKG_STATE_INSTALLED
                                     ? COL_SUCCESS : COL_TEXT_DIM),
                  LIST_RIGHT - 66, yy + 18, state_label);
        yy += ITEM_H;
    }

    int rows = list_visible_rows();
    int track_y = LIST_TOP;
    int track_height = LIST_BOTTOM - LIST_TOP;
    if (visible_len > rows && track_height > 0) {
        fill_rect(dpy, win, gc, get_color(dpy, COL_SIDEBAR),
                  SCROLLBAR_X, track_y, SCROLLBAR_W, (unsigned int)track_height);
        int thumb_height = track_height * rows / visible_len;
        if (thumb_height < 24) thumb_height = 24;
        if (thumb_height > track_height) thumb_height = track_height;
        int travel = track_height - thumb_height;
        int thumb_y = track_y;
        int max_scroll = list_max_scroll();
        if (max_scroll > 0) {
            thumb_y += travel * scroll_offset / max_scroll;
        }
        fill_rect(dpy, win, gc, get_color(dpy, COL_BUTTON),
                  SCROLLBAR_X, thumb_y, SCROLLBAR_W,
                  (unsigned int)thumb_height);
        draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER),
                  SCROLLBAR_X, thumb_y, SCROLLBAR_W - 1,
                  (unsigned int)thumb_height);
    }

    if (visible_len == 0) {
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  LIST_LEFT, LIST_TOP,
                  current_view == VIEW_GNUCHAN
                      ? center_text("No installed GnuChan programs found",
                                    "Kurulu GnuChan programi bulunamadi")
                      : center_text("No packages match this view",
                                    "Bu gorunumle eslesen paket yok"));
    }
}

static int bulk_update_button_x(int has_selected_package) {
    return LIST_LEFT + (has_selected_package ? 216 : 108);
}

static int run_queue_button_x(int has_selected_package) {
    return LIST_LEFT + (has_selected_package ? 108 : 0);
}

static int gnuchan_update_button_x(int has_selected_package) {
    return bulk_update_button_x(has_selected_package) + 148;
}

static int source_selector_x(void) {
    return WIN_WIDTH - 430;
}

static void draw_wrapped_text(Display *dpy, Window win, GC gc,
                              unsigned long color, int x, int *y,
                              const char *text, int max_rows) {
    int max_chars = (LIST_RIGHT - LIST_LEFT) / 6;
    if (max_chars < 10) max_chars = 10;
    const char *cursor = text ? text : "";
    int rows = 0;
    while (*cursor && rows < max_rows) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (!*cursor) break;
        size_t remaining = strcspn(cursor, "\r\n");
        size_t length = remaining < (size_t)max_chars
                            ? remaining : (size_t)max_chars;
        if (length == (size_t)max_chars && cursor[length] &&
            cursor[length] != ' ' && cursor[length] != '\t' &&
            cursor[length] != '\n' && cursor[length] != '\r') {
            size_t word_boundary = length;
            while (word_boundary > 0 && cursor[word_boundary] != ' ' &&
                   cursor[word_boundary] != '\t') word_boundary--;
            if (word_boundary > 0) length = word_boundary;
        }
        char line[512];
        if (length >= sizeof(line)) length = sizeof(line) - 1;
        memcpy(line, cursor, length);
        line[length] = '\0';
        draw_text(dpy, win, gc, color, x, *y, line);
        (*y) += 18;
        rows++;
        cursor += length;
        if (*cursor == '\r' || *cursor == '\n') {
            while (*cursor == '\r' || *cursor == '\n') cursor++;
        }
    }
}

static void draw_package_details(Display *dpy, Window win, GC gc) {
    if (!detail_view || selected_index < 0 || selected_index >= visible_len) {
        return;
    }
    Package *package = visible_list[selected_index].pkg;
    fill_rect(dpy, win, gc, get_color(dpy, COL_BG), LIST_LEFT, LIST_TOP,
              (unsigned int)(LIST_RIGHT - LIST_LEFT),
              (unsigned int)(LIST_BOTTOM - LIST_TOP));
    draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER), LIST_LEFT, LIST_TOP,
              (unsigned int)(LIST_RIGHT - LIST_LEFT),
              (unsigned int)(LIST_BOTTOM - LIST_TOP));

    int y = LIST_TOP + 24;
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT), LIST_LEFT + 12, y,
              package->name);
    y += 22;
    char line[512];
    snprintf(line, sizeof(line), "%s: %s    %s: %s%s%s",
             center_text("Source", "Kaynak"),
             package->source == SOURCE_FLATPAK ? "Flatpak" : "APT",
             center_text("Version", "Surum"),
             package->version[0] ? package->version
                                 : center_text("not provided", "belirtilmemis"),
             package->has_update && package->remote_version[0] ? " -> " : "",
             package->has_update ? package->remote_version : "");
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), LIST_LEFT + 12, y,
              line);
    y += 22;
    if (package->origin[0]) {
        snprintf(line, sizeof(line), "Remote: %s", package->origin);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), LIST_LEFT + 12,
                  y, line);
        y += 20;
    }
    snprintf(line, sizeof(line), "%s: %s    %s: %s",
             center_text("Maintainer", "Bakimci"),
             package->maintainer[0] ? package->maintainer
                                    : center_text("not provided",
                                                  "belirtilmemis"),
             center_text("License", "Lisans"),
             package->license[0] ? package->license
                                 : center_text(
                                       "not available in local package metadata",
                                       "yerel paket ustverisinde yok"));
    draw_wrapped_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                      LIST_LEFT + 12, &y, line, 2);
    if (package->homepage[0]) {
        snprintf(line, sizeof(line), "Homepage: %s", package->homepage);
        draw_wrapped_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                          LIST_LEFT + 12, &y, line, 2);
    }
    if (package->installed_size_kb || package->download_size_bytes) {
        snprintf(line, sizeof(line),
                 "Installed size: %lu KB    Download size: %lu KB",
                 package->installed_size_kb,
                 (package->download_size_bytes + 1023) / 1024);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), LIST_LEFT + 12,
                  y, line);
        y += 20;
    }
    snprintf(line, sizeof(line), "%s", center_text("Description",
                                                   "Aciklama"));
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT), LIST_LEFT + 12, y,
              line);
    y += 18;
    draw_wrapped_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                      LIST_LEFT + 12, &y,
                      package->desc[0] ? package->desc :
                      center_text("Description is not supplied by this catalog",
                                  "Bu katalogda aciklama bulunmuyor"),
                      5);
    snprintf(line, sizeof(line), "%s: %.490s",
             center_text("Dependencies", "Bagimliliklar"),
             package->depends[0] ? package->depends :
                      center_text("not supplied", "belirtilmemis"));
    draw_wrapped_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                      LIST_LEFT + 12, &y, line, 3);
    draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
              LIST_LEFT + 12, LIST_BOTTOM - 8,
              center_text("Press Escape or D to return to the package list",
                          "Paket listesine donmek icin Escape veya D"));
}

static void draw_footer(Display *dpy, Window win, GC gc) {
    int fy = WIN_HEIGHT - FOOTER_H;
    fill_rect(dpy, win, gc, get_color(dpy, COL_FOOTER), 0, fy,
              (unsigned int)WIN_WIDTH, (unsigned int)FOOTER_H);
    draw_rect(dpy, win, gc, get_color(dpy, COL_BORDER),
              0, (unsigned int)fy, (unsigned int)WIN_WIDTH, 1);

    Package *sel_pkg = (selected_index >= 0 && selected_index < visible_len)
        ? visible_list[selected_index].pkg : NULL;

    int bx = LIST_LEFT;
    int by = fy + 6;
    int bw = 100;
    int bh = 24;

    if (sel_pkg) {
        fill_rect(dpy, win, gc, get_color(dpy, COL_BUTTON),
                  bx, by, bw, bh);
        draw_text(dpy, win, gc, get_color(dpy, 0xFFFFFFU),
                  bx + 7, by + 18,
                  center_text("Queue action", "Kuyruga ekle"));
    }

    bx = run_queue_button_x(sel_pkg != NULL);
    fill_rect(dpy, win, gc,
              get_color(dpy, queue_count ? COL_BUTTON : COL_SELECT),
              bx, by, bw, bh);
    char queue_label[64];
    snprintf(queue_label, sizeof(queue_label),
             center_text("Run queue (%d)", "Kuyrugu calistir (%d)"),
             queue_count);
    draw_text(dpy, win, gc, get_color(dpy, 0xFFFFFFU), bx + 5, by + 18,
              queue_label);

    bx = bulk_update_button_x(sel_pkg != NULL);
    int up_w = 140;
    fill_rect(dpy, win, gc, get_color(dpy, COL_SUCCESS),
              bx, by, up_w, bh);
    draw_text(dpy, win, gc, get_color(dpy, 0xFFFFFFU),
              bx + 8, by + 18,
              current_source == SOURCE_FLATPAK
                  ? center_text("Update Flatpaks", "Flatpaklari guncelle")
                  : center_text("Check & update", "Denetle ve guncelle"));

    bx = gnuchan_update_button_x(sel_pkg != NULL);
    int gc_w = 140;
    if (current_source == SOURCE_FLATPAK) {
        fill_rect(dpy, win, gc, get_color(dpy, COL_SELECT), bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  bx + 6, by + 18, center_text("Flatpak sources via CLI",
                                                "Flatpak kaynaklari CLI"));
    } else if (gnuchan_update_count > 0) {
        fill_rect(dpy, win, gc, get_color(dpy, COL_BUTTON),
                  bx, by, gc_w, bh);
        char lbl[64];
        snprintf(lbl, sizeof(lbl), center_text("Update GnuChan (%d)",
                                               "GnuChan Guncelle (%d)"),
                 gnuchan_update_count);
        draw_text(dpy, win, gc, get_color(dpy, 0xFFFFFFU),
                  bx + 8, by + 18, lbl);
    } else if (update_checks_disabled) {
        fill_rect(dpy, win, gc, get_color(dpy, 0x392150U),
                  bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  bx + 8, by + 18,
                  center_text("Checks disabled", "Denetim kapali"));
    } else if (github_check_done && github_check_failures == 0) {
        fill_rect(dpy, win, gc, get_color(dpy, 0x392150U),
                  bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  bx + 8, by + 18,
                  center_text("GnuChan is up to date", "GnuChan Guncel"));
    } else if (github_check_done) {
        fill_rect(dpy, win, gc, get_color(dpy, COL_WARN),
                  bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_BG),
                  bx + 8, by + 18,
                  center_text("GitHub check failed",
                              "GitHub Denetlenemedi"));
    } else if (loading_packages || checking_updates) {
        fill_rect(dpy, win, gc, get_color(dpy, 0x392150U),
                  bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  bx + 8, by + 18,
                  loading_packages
                      ? center_text("Loading packages...",
                                    "Paketler yukleniyor...")
                      : center_text("Checking updates...",
                                    "Guncellemeler denetleniyor..."));
    } else {
        fill_rect(dpy, win, gc, get_color(dpy, 0x392150U),
                  bx, by, gc_w, bh);
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  bx + 8, by + 18,
                  center_text("GnuChan is up to date", "GnuChan Guncel"));
    }

    if (queue_worker_running || queue_count > 0) {
        char progress_line[320];
        if (queue_worker_running) {
            snprintf(progress_line, sizeof(progress_line),
                     "%s  %s%d%%",
                     progress_status[0] ? progress_status :
                         center_text("Working...", "Calisiyor..."),
                     progress_percent >= 0 ? "" : "",
                     progress_percent >= 0 ? progress_percent : 0);
            if (progress_percent < 0) {
                snprintf(progress_line, sizeof(progress_line), "%s",
                         progress_status[0] ? progress_status :
                             center_text("Working...", "Calisiyor..."));
            }
        } else {
            snprintf(progress_line, sizeof(progress_line),
                     center_text("%d queued action(s) pending",
                                 "%d islem kuyrukta bekliyor"), queue_count);
        }
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), LIST_LEFT,
                  fy + 43, progress_line);
        fill_rect(dpy, win, gc, get_color(dpy, COL_SIDEBAR), LIST_LEFT,
                  fy + 48, (unsigned int)(LIST_RIGHT - LIST_LEFT), 6);
        int filled = progress_percent >= 0
                         ? (LIST_RIGHT - LIST_LEFT) * progress_percent / 100
                         : (LIST_RIGHT - LIST_LEFT) / 5;
        if (filled > 0) {
            fill_rect(dpy, win, gc, get_color(dpy, COL_BUTTON), LIST_LEFT,
                      fy + 48, (unsigned int)filled, 6);
        }
    } else if (source_status[0]) {
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM), LIST_LEFT,
                  fy + 43, source_status);
    }

    if (sel_pkg && !detail_view) {
        char detail[256];
        if (sel_pkg->has_update && sel_pkg->remote_version[0]) {
            snprintf(detail, sizeof(detail),
                     "%.64s  %s%.32s -> %.32s", sel_pkg->name,
                     center_text("Version: ", "Surum: "), sel_pkg->version,
                     sel_pkg->remote_version);
        } else {
            snprintf(detail, sizeof(detail), "%.64s  %s%.32s", sel_pkg->name,
                     center_text("Version: ", "Surum: "), sel_pkg->version);
        }
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT),
                  LIST_LEFT, fy + 61, detail);

        snprintf(detail, sizeof(detail), "%.200s",
                 sel_pkg->desc[0] ? sel_pkg->desc
                                  : center_text("No description available",
                                                "Aciklama mevcut degil"));
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  LIST_LEFT, fy + 77, detail);

        snprintf(detail, sizeof(detail), "%s%s%s",
                 sel_pkg->maintainer[0]
                     ? center_text("Maintainer: ", "Bakimci: ")
                     : "",
                 sel_pkg->maintainer,
                 sel_pkg->homepage[0] ? "  " : "");
        size_t used = strlen(detail);
        if (sel_pkg->installed_size_kb || sel_pkg->download_size_bytes) {
            int added = snprintf(detail + used, sizeof(detail) - used,
                                 "%s%lu KB installed / %lu KB download",
                                 used ? "  " : "",
                                 sel_pkg->installed_size_kb,
                                 (sel_pkg->download_size_bytes + 1023) / 1024);
            if (added > 0 && (size_t)added < sizeof(detail) - used) {
                used += (size_t)added;
            }
        }
        if (sel_pkg->homepage[0] && used < sizeof(detail) - 1) {
            snprintf(detail + used, sizeof(detail) - used, "%s%s",
                     used ? "  " : "", sel_pkg->homepage);
        }
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  LIST_LEFT, fy + 93, detail);
    }

    if (status_msg[0]) {
        draw_text(dpy, win, gc, get_color(dpy, COL_TEXT_DIM),
                  LIST_LEFT, fy + FOOTER_H - 8, status_msg);
    }
}

static void redraw(Display *dpy, Window win, GC gc) {
    if (back_buffer == None || back_buffer_width != WIN_WIDTH ||
        back_buffer_height != WIN_HEIGHT) {
        if (back_buffer != None) XFreePixmap(dpy, back_buffer);
        back_buffer = XCreatePixmap(dpy, win, (unsigned int)WIN_WIDTH,
                                    (unsigned int)WIN_HEIGHT,
                                    (unsigned int)DefaultDepth(dpy,
                                                               DefaultScreen(dpy)));
        back_buffer_width = WIN_WIDTH;
        back_buffer_height = WIN_HEIGHT;
    }
    fill_rect(dpy, back_buffer, gc, get_color(dpy, COL_BG), 0, 0,
              (unsigned int)WIN_WIDTH, (unsigned int)WIN_HEIGHT);
    draw_sidebar(dpy, back_buffer, gc);
    draw_header(dpy, back_buffer, gc);
    draw_package_list(dpy, back_buffer, gc);
    draw_package_details(dpy, back_buffer, gc);
    draw_footer(dpy, back_buffer, gc);
    XCopyArea(dpy, back_buffer, win, gc, 0, 0, (unsigned int)WIN_WIDTH,
              (unsigned int)WIN_HEIGHT, 0, 0);
    XFlush(dpy);
}

static void set_scroll_from_pointer(int y) {
    int track_y = LIST_TOP;
    int track_height = LIST_BOTTOM - LIST_TOP;
    int rows = list_visible_rows();
    int thumb_height = track_height * rows / visible_len;
    if (thumb_height < 24) thumb_height = 24;
    if (thumb_height > track_height) thumb_height = track_height;
    int travel = track_height - thumb_height;
    int max_scroll = list_max_scroll();
    if (travel <= 0 || max_scroll <= 0) {
        scroll_offset = 0;
        return;
    }
    int thumb_y = y - track_y - thumb_height / 2;
    if (thumb_y < 0) thumb_y = 0;
    if (thumb_y > travel) thumb_y = travel;
    scroll_offset = thumb_y * max_scroll / travel;
    selected_index = -1;
}

/* --- olay dongusu -------------------------------------------------------- */

static void handle_keypress(Display *dpy, Window win, GC gc, XKeyEvent *ev) {
    KeySym ks = XLookupKeysym(ev, 0);
    char buf[16];
    int n = XLookupString(ev, buf, sizeof(buf), &ks, NULL);

    if (busy) return;

    if (ev->state & ControlMask) {
        ViewId shortcut_view;
        int has_view = 1;
        if (ks == XK_f || ks == XK_F) {
            search_focused = 1;
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
            redraw(dpy, win, gc);
            return;
        } else if (ks == XK_i || ks == XK_I) {
            shortcut_view = VIEW_INSTALLED;
        } else if (ks == XK_u || ks == XK_U) {
            shortcut_view = VIEW_UPDATES;
        } else {
            has_view = 0;
        }
        if (has_view) {
            current_view = shortcut_view;
            selected_index = -1;
            scroll_offset = 0;
            rebuild_visible_list();
            redraw(dpy, win, gc);
            return;
        }
    }

    if (ks == XK_Escape) {
        if (detail_view) {
            detail_view = 0;
            redraw(dpy, win, gc);
            return;
        }
        search_text[0] = '\0';
        search_focused = 0;
        rebuild_visible_list();
        selected_index = visible_len > 0 ? 0 : -1;
        redraw(dpy, win, gc);
        return;
    }

    if (search_focused && ks == XK_BackSpace) {
        int len = (int)strlen(search_text);
        if (len > 0) {
            search_text[len - 1] = '\0';
            rebuild_visible_list();
            redraw(dpy, win, gc);
        }
        return;
    }

    if (!search_focused && (ks == XK_d || ks == XK_D) &&
        selected_index >= 0 && selected_index < visible_len) {
        detail_view = !detail_view;
        if (detail_view) {
            request_package_details(visible_list[selected_index].pkg);
        }
        redraw(dpy, win, gc);
        return;
    }

    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (search_focused) {
            search_focused = 0;
            redraw(dpy, win, gc);
            return;
        }
        if (loading_packages || checking_updates) return;
        if (selected_index >= 0 && selected_index < visible_len) {
            Package *p = visible_list[selected_index].pkg;
            QueueAction action = p->state == PKG_STATE_INSTALLED
                ? (p->has_update ? QUEUE_UPDATE : QUEUE_REMOVE)
                : QUEUE_INSTALL;
            enqueue_operation(action, p->source, p->name, p->origin);
            start_queue();
            redraw(dpy, win, gc);
        }
        return;
    }

    if (ks == XK_Up) {
        if (selected_index > 0) selected_index--;
        if (selected_index < scroll_offset) scroll_offset = selected_index;
        rebuild_visible_list();
        redraw(dpy, win, gc);
        return;
    }
    if (ks == XK_Down) {
        if (selected_index < visible_len - 1) selected_index++;
        int item_visible = (LIST_BOTTOM - LIST_TOP) / ITEM_H + 1;
        if (selected_index >= scroll_offset + item_visible)
            scroll_offset = selected_index - item_visible + 1;
        rebuild_visible_list();
        redraw(dpy, win, gc);
        return;
    }
    if (ks == XK_Page_Up) {
        int item_visible = (LIST_BOTTOM - LIST_TOP) / ITEM_H + 1;
        selected_index -= item_visible;
        if (selected_index < 0) selected_index = 0;
        scroll_offset = selected_index;
        rebuild_visible_list();
        redraw(dpy, win, gc);
        return;
    }
    if (ks == XK_Page_Down) {
        int item_visible = (LIST_BOTTOM - LIST_TOP) / ITEM_H + 1;
        selected_index += item_visible;
        if (selected_index >= visible_len) selected_index = visible_len - 1;
        scroll_offset = selected_index - item_visible + 1;
        rebuild_visible_list();
        redraw(dpy, win, gc);
        return;
    }

    if (search_focused && n > 0 && isprint((unsigned char)buf[0])) {
        int len = (int)strlen(search_text);
        if (len < (int)sizeof(search_text) - 1) {
            search_text[len] = buf[0];
            search_text[len + 1] = '\0';
            rebuild_visible_list();
            selected_index = visible_len > 0 ? 0 : -1;
            scroll_offset = 0;
            redraw(dpy, win, gc);
        }
    }
}

static void handle_button(Display *dpy, Window win, GC gc, XButtonEvent *ev) {
    if (busy) return;

    int x = ev->x, y = ev->y;

    if ((ev->button == Button4 || ev->button == Button5) &&
        y >= LIST_TOP && y <= LIST_BOTTOM &&
        x >= LIST_LEFT && x <= SCROLLBAR_X + SCROLLBAR_W) {
        int max_scroll = list_max_scroll();
        scroll_offset += ev->button == Button4 ? -3 : 3;
        if (scroll_offset < 0) scroll_offset = 0;
        if (scroll_offset > max_scroll) scroll_offset = max_scroll;
        selected_index = -1;
        redraw(dpy, win, gc);
        return;
    }

    if (x >= source_selector_x() && x < source_selector_x() + 112 &&
        y >= 10 && y < 34) {
        detail_view = 0;
        current_view = VIEW_ALL;
        select_package_source(current_source == SOURCE_APT
                                  ? SOURCE_FLATPAK : SOURCE_APT);
        redraw(dpy, win, gc);
        return;
    }

    if (x >= WIN_WIDTH - 300 && x < WIN_WIDTH - 20 &&
        y >= 10 && y < 34) {
        search_focused = 1;
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        redraw(dpy, win, gc);
        return;
    }

    if (search_focused) {
        search_focused = 0;
        redraw(dpy, win, gc);
    }

    if (x < SIDEBAR_W && y > HEADER_H) {
        int yy = HEADER_H + 20;
        for (int i = 0; i < sidebar_count; i++) {
            if (y >= yy - 4 && y < yy - 4 + 30) {
                current_view = sidebar_items[i].view;
                selected_index = -1;
                scroll_offset = 0;
                rebuild_visible_list();
                redraw(dpy, win, gc);
                return;
            }
            yy += 30;
        }
        return;
    }

    if (x >= SCROLLBAR_X && x < SCROLLBAR_X + SCROLLBAR_W &&
        y >= LIST_TOP && y <= LIST_BOTTOM && visible_len > list_visible_rows()) {
        set_scroll_from_pointer(y);
        scrollbar_dragging = 1;
        redraw(dpy, win, gc);
        return;
    }

    if (x >= LIST_LEFT && x < LIST_RIGHT &&
        y >= LIST_TOP && y <= LIST_BOTTOM) {
        int rel_y = y - LIST_TOP + scroll_offset * ITEM_H;
        int idx = rel_y / ITEM_H;
        if (idx >= 0 && idx < visible_len) {
            selected_index = idx;
            redraw(dpy, win, gc);
        }
        return;
    }

    if (loading_packages || checking_updates) return;

    int fy = WIN_HEIGHT - FOOTER_H + 6;
    if (y >= fy && y < fy + 24) {
        Package *sel_pkg = (selected_index >= 0 && selected_index < visible_len)
            ? visible_list[selected_index].pkg : NULL;

        if (sel_pkg && x >= LIST_LEFT && x < LIST_LEFT + 100) {
            QueueAction action = sel_pkg->state == PKG_STATE_INSTALLED
                ? (sel_pkg->has_update ? QUEUE_UPDATE : QUEUE_REMOVE)
                : QUEUE_INSTALL;
            enqueue_operation(action, sel_pkg->source, sel_pkg->name,
                              sel_pkg->origin);
            redraw(dpy, win, gc);
            return;
        }

        int queue_x = run_queue_button_x(sel_pkg != NULL);
        if (x >= queue_x && x < queue_x + 100) {
            start_queue();
            redraw(dpy, win, gc);
            return;
        }

        int bx = bulk_update_button_x(sel_pkg != NULL);
        if (x >= bx && x < bx + 140) {
            enqueue_operation(QUEUE_UPDATE_ALL, current_source, "", "");
            start_queue();
            redraw(dpy, win, gc);
            return;
        }

        bx = gnuchan_update_button_x(sel_pkg != NULL);
        if (x >= bx && x < bx + 140 && gnuchan_update_count > 0) {
            enqueue_operation(QUEUE_GNUCHAN_UPDATE, SOURCE_APT, "", "");
            start_queue();
            redraw(dpy, win, gc);
            return;
        }
    }
}

static void handle_motion(Display *dpy, Window win, GC gc, XMotionEvent *ev) {
    if (!scrollbar_dragging) return;
    set_scroll_from_pointer(ev->y);
    redraw(dpy, win, gc);
}

/* --- ana ----------------------------------------------------------------- */

static void handle_signal(int signum) {
    (void)signum;
    should_stop = 1;
}

static void *initial_load_worker(void *arg) {
    const char *home = arg;
    if (current_source == SOURCE_FLATPAK) {
        load_flatpak_packages();
    } else {
        load_installed_packages();
        add_user_gnuchan_programs(home);
    }

    pthread_mutex_lock(&state_mutex);
    checking_updates = current_source == SOURCE_APT && check_updates_on_start;
    if (!check_updates_on_start) {
        update_checks_disabled = 1;
        github_check_done = 1;
    }
    loading_packages = 0;
    set_status(check_updates_on_start
                   ? center_text(
                         "Packages loaded; checking for updates in the background",
                         "Paketler yuklendi; guncellemeler arka planda denetleniyor")
                   : center_text(
                         "Packages loaded; automatic update checks are disabled",
                         "Paketler yuklendi; otomatik guncelleme denetimi kapali"));
    rebuild_visible_list();
    state_revision++;
    pthread_mutex_unlock(&state_mutex);

    if (current_source == SOURCE_APT) {
        refresh_update_checks(home);
    }
    return NULL;
}

int main(int argc, char **argv) {
    gnuchan_lang_init();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanSoftwareCenter %s\n", GNUCHANSOFTWARECENTER_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("%s\n", center_text(
                "Usage: GnuChanSoftwareCenter [--version] [--help]\n"
                "       --list-flatpak-remotes\n"
                "       --add-flatpak-remote NAME URL\n"
                "       --remove-flatpak-remote NAME\n"
                "\nGnuchanOS package manager. Lists Debian packages, checks for\n"
                "updates, and updates installed GnuChan programs.\n",
                "Kullanim: GnuChanSoftwareCenter [--version] [--help]\n"
                "\nGnuchanOS paket yoneticisi. Debian paketlerini listeler,\n"
                "guncellemeleri denetler ve GnuChan programlarini gunceller.\n"));
            return 0;
        }
        if (strcmp(argv[i], "--list-flatpak-remotes") == 0) {
            flatpak_available =
                run_cmd("command -v flatpak >/dev/null 2>&1",
                        (char[2]){0}, 2) == 0;
            if (!flatpak_available || read_flatpak_remote_list() != 0) {
                fprintf(stderr, "Flatpak CLI unavailable or remote listing failed\n");
                return 1;
            }
            for (int remote = 0; remote < flatpak_remote_count; remote++) {
                puts(flatpak_remotes[remote]);
            }
            return 0;
        }
        if (strcmp(argv[i], "--add-flatpak-remote") == 0) {
            if (i + 2 >= argc || !valid_flatpak_token(argv[i + 1]) ||
                !valid_flatpak_url(argv[i + 2])) {
                fprintf(stderr, "Usage: %s --add-flatpak-remote NAME URL\n",
                        argv[0]);
                return 2;
            }
            flatpak_available =
                run_cmd("command -v flatpak >/dev/null 2>&1",
                        (char[2]){0}, 2) == 0;
            return manage_flatpak_remote(1, argv[i + 1], argv[i + 2]) == 0
                       ? 0 : 1;
        }
        if (strcmp(argv[i], "--remove-flatpak-remote") == 0) {
            if (i + 1 >= argc || !valid_flatpak_token(argv[i + 1])) {
                fprintf(stderr, "Usage: %s --remove-flatpak-remote NAME\n",
                        argv[0]);
                return 2;
            }
            flatpak_available =
                run_cmd("command -v flatpak >/dev/null 2>&1",
                        (char[2]){0}, 2) == 0;
            return manage_flatpak_remote(0, argv[i + 1], NULL) == 0 ? 0 : 1;
        }
    }

    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        home = "/root";
    }
    load_center_settings(home);
    if (show_installed_by_default) {
        current_view = VIEW_INSTALLED;
    }

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&state_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    /* --- X11 pencere once acilir, arkaplan islemleri sonra ----------------- */

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "GnuChanSoftwareCenter: X display acilamadi\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    Window win = XCreateSimpleWindow(dpy, root, 60, 60,
                                     (unsigned int)window_width,
                                     (unsigned int)window_height,
                                     1, BlackPixel(dpy, screen),
                                     get_color(dpy, COL_BG));
    XSizeHints size_hints = {0};
    size_hints.flags = PMinSize;
    size_hints.min_width = 720;
    size_hints.min_height = 480;
    XSetWMNormalHints(dpy, win, &size_hints);
    XSelectInput(dpy, win,
                 ExposureMask | KeyPressMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "GnuChanSoftwareCenter");
    XFlush(dpy);

    /* Wait for the first Expose event so the initial loading screen is visible. */
    XEvent ev;
    while (XPending(dpy) == 0) {
        struct timespec ts = {0, 10 * 1000 * 1000L};
        nanosleep(&ts, NULL);
    }
    XNextEvent(dpy, &ev);

    GC gc = create_gc(dpy, win);
    redraw(dpy, win, gc);

    pthread_t loader_thread;
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    int thread_result = pthread_create(&loader_thread, &thread_attr,
                                       initial_load_worker, (void *)home);
    pthread_attr_destroy(&thread_attr);
    if (thread_result != 0) {
        initial_load_worker((void *)home);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    pthread_mutex_lock(&state_mutex);
    unsigned long drawn_revision = state_revision;
    pthread_mutex_unlock(&state_mutex);
    while (!should_stop) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            pthread_mutex_lock(&state_mutex);
            switch (ev.type) {
            case Expose:
                redraw(dpy, win, gc);
                break;
            case KeyPress:
                handle_keypress(dpy, win, gc, &ev.xkey);
                break;
            case ButtonPress:
                handle_button(dpy, win, gc, &ev.xbutton);
                break;
            case ButtonRelease:
                scrollbar_dragging = 0;
                break;
            case MotionNotify:
                handle_motion(dpy, win, gc, &ev.xmotion);
                break;
            case ConfigureNotify:
                if (window_width != ev.xconfigure.width ||
                    window_height != ev.xconfigure.height) {
                    window_width = ev.xconfigure.width;
                    window_height = ev.xconfigure.height;
                    rebuild_visible_list();
                    redraw(dpy, win, gc);
                }
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&state_mutex);
        } else {
            struct timespec ts = {0, 50 * 1000 * 1000L};
            nanosleep(&ts, NULL);
        }

        pthread_mutex_lock(&state_mutex);
        if (drawn_revision != state_revision) {
            redraw(dpy, win, gc);
            drawn_revision = state_revision;
        }
        pthread_mutex_unlock(&state_mutex);
    }

    XFreeGC(dpy, gc);
    if (back_buffer != None) XFreePixmap(dpy, back_buffer);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
