#ifndef TERM_PROMPT_H
#define TERM_PROMPT_H

#include <stddef.h>

int term_prompt_is_zsh(const char *shell);
void term_prompt_for_shell(const char *prompt, const char *shell,
                           char *out, size_t out_size);

#endif
