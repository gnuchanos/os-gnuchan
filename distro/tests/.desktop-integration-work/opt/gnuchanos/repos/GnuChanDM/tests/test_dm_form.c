#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dm_core.h"

static void type_text(DmCore *core, const char *text) {
    while (*text) dm_form_type(core, *text++);
}

int main(void) {
    DmCore core = {0};
    core.focus = DM_FOCUS_USERNAME;

    type_text(&core, "alice-admin 42");
    assert(strcmp(core.username, "alice-admin 42") == 0);
    assert(core.field_cursor[DM_FIELD_USERNAME] == core.username_length);

    dm_form_cursor_home(&core);
    type_text(&core, "x");
    assert(strcmp(core.username, "xalice-admin 42") == 0);
    dm_form_cursor_right(&core, 0);
    dm_form_type(&core, 'y');
    assert(strcmp(core.username, "xaylice-admin 42") == 0);

    dm_form_cursor_end(&core);
    dm_form_cursor_left(&core, 1);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 14);
    dm_form_cursor_left(&core, 1);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 8);
    dm_form_cursor_right(&core, 1);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 14);
    dm_form_cursor_right(&core, 1);
    assert(core.field_cursor[DM_FIELD_USERNAME] == core.username_length);

    dm_form_select_all(&core);
    dm_form_type(&core, 'u');
    assert(strcmp(core.username, "u") == 0);
    dm_form_cursor_home(&core);
    dm_form_delete(&core);
    assert(core.username_length == 0);
    dm_form_delete(&core);
    assert(core.username_length == 0);

    type_text(&core, "abc");
    dm_form_cursor_home(&core);
    dm_form_delete(&core);
    assert(strcmp(core.username, "bc") == 0);
    dm_form_cursor_end(&core);
    dm_form_backspace(&core);
    assert(strcmp(core.username, "b") == 0);
    dm_form_backspace(&core);
    dm_form_backspace(&core);
    assert(core.username_length == 0);

    dm_form_type(&core, '\n');
    dm_form_type(&core, (char)0xc3);
    assert(core.username_length == 0);
    type_text(&core, "reset");
    dm_form_cursor_end(&core);
    dm_form_clear_username(&core);
    assert(core.username_length == 0);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 0);
    dm_form_cursor_left(&core, 1);
    assert(core.field_cursor[DM_FIELD_USERNAME] == 0);

    core.focus = DM_FOCUS_PASSWORD;
    type_text(&core, "s3cr3t");
    char rendered[DM_MAX_PASSWORD];
    int rendered_length = dm_form_copy_display_text(
        &core, DM_FIELD_PASSWORD, rendered, sizeof(rendered));
    assert(rendered_length == 6);
    assert(strcmp(rendered, "******") == 0);
    assert(strstr(rendered, "s3cr3t") == NULL);

    dm_form_cursor_home(&core);
    dm_form_delete(&core);
    assert(strcmp(core.password, "3cr3t") == 0);
    dm_form_select_all(&core);
    dm_form_backspace(&core);
    assert(core.password_length == 0);
    dm_form_clear_password(&core);
    assert(core.password[0] == '\0');

    core.focus = DM_FOCUS_USERNAME;
    assert(dm_form_focus_next(&core) == DM_FOCUS_PASSWORD);
    assert(dm_form_focus_next(&core) == DM_FOCUS_SIGNIN);
    assert(dm_form_focus_prev(&core) == DM_FOCUS_PASSWORD);

    puts("form editing tests passed");
    return 0;
}
