#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/runner_apps.h"
#include "../src/runner_desktop.c"
#include "../src/runner_key_state.h"

static int check(int condition, const char *description) {
    if (!condition) {
        fprintf(stderr, "failed: %s\n", description);
        return 0;
    }
    return 1;
}

static int has_program(const RunnerProgramList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int has_program_command(const RunnerProgramList *list, const char *name,
                               const char *command) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0 &&
            strcmp(list->items[i].command[0], command) == 0) {
            return 1;
        }
    }
    return 0;
}

static int check_discovery(void) {
    if (setenv("HOME", "tests/fixtures/runner-discovery/home", 1) != 0 ||
        setenv("XDG_DATA_DIRS",
               "tests/fixtures/runner-discovery/system:"
               "tests/fixtures/runner-discovery/xdg-extra", 1) != 0 ||
        unsetenv("XDG_DATA_HOME") != 0) {
        return check(0, "sets isolated XDG paths for discovery test");
    }

    RunnerConfig config = {0};
    config.desktop_dir_count = 1;
    snprintf(config.desktop_dirs[0], sizeof(config.desktop_dirs[0]),
             "tests/fixtures/runner-discovery/custom");
    config.extra_program_count = 1;
    snprintf(config.extra_programs[0].name,
             sizeof(config.extra_programs[0].name), "Manual Utility");
    snprintf(config.extra_programs[0].command,
             sizeof(config.extra_programs[0].command), "manual-utility");

    RunnerProgramList list;
    runner_apps_init(&list);
    runner_apps_load(&list, &config);

    int passed =
        check(has_program(&list, "System Catalog App"),
              "finds apps from XDG_DATA_DIRS") &&
        check(has_program(&list, "Extra XDG Catalog App"),
              "finds apps from every XDG_DATA_DIRS root") &&
        check(has_program_command(&list, "GnuChan Desktop",
                                  "gnuchan-desktop-panel"),
              "finds the /usr/local-installed GnuChan Desktop app by default") &&
        check(has_program(&list, "System Fallback Catalog App"),
              "includes the /usr-style fallback by default") &&
        check(has_program(&list, "User Catalog App"),
              "finds apps installed for the current user") &&
        check(has_program_command(&list, "User Priority App",
                                  "user-priority-command"),
              "prefers the user's desktop entry when names collide") &&
        check(has_program(&list, "Custom Catalog App"),
              "adds DesktopDir entries without replacing installed apps") &&
        check(has_program(&list, "Manual Utility"),
              "keeps manually configured programs alongside discovered apps");
    runner_apps_free(&list);
    return passed;
}

int main(void) {
    if (!check_discovery()) {
        return 1;
    }

    RunnerKeyState key_state = {{0}};
    char keymap[RUNNER_KEYMAP_SIZE] = {0};
    const unsigned int shortcut_keycode = 38;
    const unsigned int modifier_keycode = 64;
    keymap[shortcut_keycode / 8] |=
        (char)(1u << (shortcut_keycode % 8));
    keymap[modifier_keycode / 8] |=
        (char)(1u << (modifier_keycode % 8));
    runner_key_state_capture(&key_state, keymap);
    if (!check(runner_key_state_ignores_press(&key_state, shortcut_keycode),
               "ignores the shortcut key held when the launcher opens") ||
        !check(runner_key_state_ignores_press(&key_state, modifier_keycode),
               "ignores the held Alt modifier") ||
        !check(!runner_key_state_ignores_press(&key_state, 65),
               "does not ignore keys that were not held at launch")) {
        return 1;
    }
    runner_key_state_release(&key_state, shortcut_keycode, 1);
    if (!check(runner_key_state_ignores_press(&key_state, shortcut_keycode),
               "keeps ignoring a key across an auto-repeat release")) {
        return 1;
    }
    runner_key_state_release(&key_state, shortcut_keycode, 0);
    if (!check(!runner_key_state_ignores_press(&key_state, shortcut_keycode),
               "accepts a key again after its real release")) {
        return 1;
    }

    char generic[RUNNER_TEXT_LENGTH];
    memset(generic, 'g', sizeof(generic) - 1);
    generic[sizeof(generic) - 1] = '\0';

    char combined[RUNNER_TEXT_LENGTH];
    set_keywords(combined, sizeof(combined), generic, "searchneedle");
    if (!check(strlen(combined) < sizeof(combined),
               "keeps the combined field terminated") ||
        !check(strstr(combined, "searchneedle") != NULL,
               "retains keywords when generic name fills the field")) {
        return 1;
    }

    set_keywords(combined, sizeof(combined), "Utilities", "search");
    if (!check(strcmp(combined, "Utilities search") == 0,
               "preserves ordinary generic names and keywords")) {
        return 1;
    }

    char tiny[3];
    set_keywords(tiny, sizeof(tiny), "Utilities", "search");
    if (!check(strcmp(tiny, "s") == 0,
               "does not spend a tiny buffer on a dangling separator")) {
        return 1;
    }

    puts("runner keyword tests passed");
    return 0;
}
