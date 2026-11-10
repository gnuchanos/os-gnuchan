#include <assert.h>
#include <string.h>

#include "../term_prompt.h"

int main(void) {
    const char bash_prompt[] =
        "\\[\\e[38;5;135m\\]\\u@\\h"
        "\\[\\e[0m\\]:\\w\\$ ";
    char actual[256];
    char expected[] =
        "%{\033[38;5;135m%}%n@%m"
        "%{\033[0m%}:%~%# ";

    assert(term_prompt_is_zsh("/bin/zsh"));
    assert(term_prompt_is_zsh("zsh"));
    assert(!term_prompt_is_zsh("/bin/bash"));
    assert(!term_prompt_is_zsh("/bin/zsh-custom"));

    term_prompt_for_shell(bash_prompt, "/bin/zsh", actual, sizeof(actual));
    assert(strcmp(actual, expected) == 0);

    term_prompt_for_shell(bash_prompt, "/bin/bash", actual, sizeof(actual));
    assert(strcmp(actual, bash_prompt) == 0);

    term_prompt_for_shell(bash_prompt, NULL, actual, sizeof(actual));
    assert(strcmp(actual, bash_prompt) == 0);

    term_prompt_for_shell("100% \\[\\e[0m\\]", "/bin/zsh",
                          actual, sizeof(actual));
    assert(strcmp(actual, "100%% %{\033[0m%}") == 0);

    return 0;
}
