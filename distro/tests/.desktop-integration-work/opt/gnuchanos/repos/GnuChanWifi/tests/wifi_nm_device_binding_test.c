#include "../src/wifi_nm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static char commands[4][WIFI_TEXT * 20];
static int command_count;

const char *wifi_shell_program_quoted(void)
{
    return "nmcli";
}

void wifi_shell_quote(const char *in, char *out, unsigned int size)
{
    if (size == 0) {
        return;
    }
    snprintf(out, size, "'%s'", in ? in : "");
}

int wifi_shell_run(const char *command, char *out, unsigned int size)
{
    if (command_count < 4) {
        snprintf(commands[command_count], sizeof(commands[command_count]),
                 "%s", command);
    }
    command_count++;
    if (size) {
        out[0] = '\0';
        if (strstr(command, "device wifi list")) {
            snprintf(out, size, "Cafe:72:WPA2:*\n");
        }
    }
    return 0;
}

const char *wifi_shell_split(const char *line, char *out, unsigned int size)
{
    unsigned int at = 0;
    if (!line || size == 0) {
        if (size) {
            out[0] = '\0';
        }
        return NULL;
    }
    while (*line && *line != '\n' && *line != ':') {
        if (at + 1 < size) {
            out[at++] = *line;
        }
        line++;
    }
    out[at] = '\0';
    return *line == ':' ? line + 1 : NULL;
}

int main(void)
{
    WifiList list;
    const char saved[1][WIFI_TEXT] = {{0}};
    assert(wifi_nm_scan(&list, "wlan1", saved, 0) == 0);
    assert(list.count == 1);
    assert(strcmp(list.items[0].ssid, "Cafe") == 0);
    assert(command_count == 2);
    assert(strstr(commands[0], "device wifi rescan ifname 'wlan1'"));
    assert(strstr(commands[1], "device wifi list ifname 'wlan1'"));

    command_count = 0;
    char error[WIFI_TEXT];
    assert(wifi_nm_connect("wlan1", "Cafe", "secret", error,
                           sizeof(error)) == 0);
    assert(command_count == 1);
    assert(strstr(commands[0], "device wifi connect 'Cafe' password 'secret' "
                               "ifname 'wlan1'"));

    command_count = 0;
    assert(wifi_nm_connect("wlan1", "Open Cafe", NULL, error,
                           sizeof(error)) == 0);
    assert(command_count == 1);
    assert(strstr(commands[0], "device wifi connect 'Open Cafe' "
                               "ifname 'wlan1'"));

    return 0;
}
