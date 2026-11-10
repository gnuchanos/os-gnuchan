#include <stdio.h>
#include <string.h>

#include "term_prompt.h"

int term_prompt_is_zsh(const char *shell) {
    if (shell == NULL || shell[0] == '\0') {
        return 0;
    }

    const char *name = strrchr(shell, '/');
    name = name != NULL ? name + 1 : shell;
    return strcmp(name, "zsh") == 0;
}

void term_prompt_for_shell(const char *prompt, const char *shell,
                           char *out, size_t out_size) {
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (prompt == NULL) {
        return;
    }
    if (!term_prompt_is_zsh(shell)) {
        snprintf(out, out_size, "%s", prompt);
        return;
    }

    size_t used = 0;
    for (size_t i = 0; prompt[i] != '\0' && used + 1 < out_size; i++) {
        const char *replacement = NULL;
        char literal[2] = { prompt[i], '\0' };

        if (prompt[i] == '\\' && prompt[i + 1] != '\0') {
            switch (prompt[i + 1]) {
            case '[': replacement = "%{"; break;
            case ']': replacement = "%}"; break;
            case 'e': literal[0] = '\033'; break;
            case 'u': replacement = "%n"; break;
            case 'h': replacement = "%m"; break;
            case 'H': replacement = "%M"; break;
            case 'w': replacement = "%~"; break;
            case 'W': replacement = "%1~"; break;
            case '$': replacement = "%#"; break;
            case 'n': literal[0] = '\n'; break;
            case 'r': literal[0] = '\r'; break;
            default:
                break;
            }
            if (replacement != NULL || prompt[i + 1] == 'e' ||
                prompt[i + 1] == 'n' || prompt[i + 1] == 'r') {
                i++;
            } else {
                literal[0] = '\\';
            }
        } else if (prompt[i] == '%') {
            replacement = "%%";
        }

        if (replacement != NULL) {
            size_t length = strlen(replacement);
            if (used + length >= out_size) {
                break;
            }
            memcpy(out + used, replacement, length);
            used += length;
        } else {
            out[used++] = literal[0];
            if (literal[0] == '\\' && prompt[i] == '\\' &&
                prompt[i + 1] != '\0' && used + 1 < out_size) {
                out[used++] = prompt[++i];
            }
        }
    }
    out[used] = '\0';
}
