/*
 * net_manager.c — the entry point.
 *
 * The manager is one window and one purpose: see every network interface the
 * machine has, bring one up or down, set the resolver, and open the wifi manager
 * for the wireless one. Everything it is made of is in the files beside this one
 * — the settings by net_config.c, the shell by net_shell.c, the interfaces by
 * net_device.c, the resolver by net_dns.c, the window by net_ui.c — and what is
 * left for this file is the order to call them in.
 *
 * GnuChanNetworkManager            open the manager
 * GnuChanNetworkManager --config F read the settings at F
 * GnuChanNetworkManager --list     print the interfaces
 * GnuChanNetworkManager --version  print the version and exit
 *
 * --list is the one thing to run without a window, and it is there on purpose:
 * a manager that will not open a window on a machine with a broken driver is
 * one whose diagnosis is a guess. --list prints the interfaces, their state,
 * their addresses, as text.
 */
#include <stdio.h>
#include <string.h>

#include "net_config.h"
#include "net_device.h"
#include "net_dpi.h"
#include "net_privilege.h"
#include "net_ui.h"
#include "gnuchan_lang.h"

#define GNUCHANNET_VERSION "0.1.0"

/* Print the interfaces and the resolver, as text. */
static int list_state(const char *config_path) {
    NetConfig config;
    char found[NET_TEXT * 2];
    const char *path = config_path;

    if (!path || !path[0]) {
        path = net_config_path(found, sizeof(found));
    }
    if (net_config_load(&config, path) != 0) {
        fprintf(stderr, "gnuchannetworkmanager: %s; %s\n",
                config.error, gnuchan_lang_t("runner_using_defaults"));
    }

    NetDeviceList devices;
    if (net_device_list(&devices, &config) != 0) {
        fprintf(stderr,
                "gnuchannetworkmanager: neither nmcli nor ip could be run\n");
        return 1;
    }

    printf("interfaces: %d\n", devices.count);
    for (int i = 0; i < devices.count; i++) {
        NetDevice *device = &devices.items[i];
        const char *state = device->state == 1 ? "connected"
                                                 : (device->up ? "up" : "down");
        printf("  %-16s %-8s %-12s %-16s %s\n",
               device->name,
               device->type[0] ? device->type : "-",
               state,
               device->address[0] ? device->address : "-",
               device->connection[0] ? device->connection : "");
    }

    char wifi[NET_TEXT];
    net_device_first_wifi(&devices, wifi, sizeof(wifi));
    printf("wireless: %s\n", wifi[0] ? wifi : "(none)");

    /* The DPI bypass: whether one is installed to switch on, and whether it is
       running now — the same two facts the button shows. */
    if (net_dpi_available(&config)) {
        printf("dpi: %s\n", net_dpi_active(&config) ? "on" : "off");
    } else {
        printf("dpi: not installed\n");
    }
    return 0;
}

static void print_help(void) {
    printf("usage: GnuChanNetworkManager [--config FILE] [--list] "
           "[--version] [--help]\n"
           "\n"
           "Opens the network manager: every interface at the top. Click a row to\n"
           "choose it, then press Up/Down to bring it up or down, Open Wi-Fi to\n"
           "start the wifi manager.\n"
           "\n"
           "  Up / Down     move through the interfaces\n"
           "  Enter         bring the chosen interface up or down\n"
           "  w             open the wifi manager\n"
           "  p             start or stop the DPI bypass (zapret)\n"
           "  r             read the interfaces again\n"
           "  Escape        close\n"
           "\n"
           "  --config FILE  read the settings at FILE instead of the one\n"
           "                 under ~/.config/GnuChanNetworkManager/\n"
           "  --list         print the interfaces and exit\n"
           "  --version      print the version and exit\n");
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    int list_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanNetworkManager %s\n", GNUCHANNET_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--list") == 0) {
            list_only = 1;
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr,
                        "gnuchannetworkmanager: --config needs a file name\n");
                return 2;
            }
            config_path = argv[++i];
            continue;
        }
        fprintf(stderr, "gnuchannetworkmanager: unknown option '%s'\n",
                argv[i]);
        print_help();
        return 2;
    }

    gnuchan_lang_init();

    if (list_only) {
        return list_state(config_path);
    }

    /* The settings are read here — before the password window — because that
       window is drawn in them, so the one thing a person sees before the
       manager opens looks like the manager. */
    NetConfig config;
    char found[NET_TEXT * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = net_config_path(found, sizeof(found));
    }
    if (net_config_load(&config, path) != 0) {
        fprintf(stderr, "gnuchannetworkmanager: %s; %s\n",
                config.error, gnuchan_lang_t("runner_using_defaults"));
    }

    /* Opening the window is what needs root: writing /etc/resolv.conf and
       bringing an interface up are both root's to do, and a window that
       half-works because each action is refused is worse than asking for the
       password once. net_privilege_ensure() shows the password window, re-runs
       this program through sudo, and exits with the copy; a machine with no
       display or no sudo carries on here and the privileged actions simply
       report that they failed. */
    if (net_privilege_ensure(argc, argv, &config) != 0) {
        fprintf(stderr,
                "gnuchannetworkmanager: carrying on without root; the interfaces "
                "cannot be changed\n");
    }

    NetUi ui;
    if (net_ui_open(&ui, config_path) != 0) {
        net_ui_close(&ui);
        return 1;
    }
    net_ui_run(&ui);
    net_ui_close(&ui);
    return 0;
}
