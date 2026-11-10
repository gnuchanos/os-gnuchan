/*
 * dm_form.c — the login form's state: what was typed, where the cursor is.
 */
#include <stdio.h>
#include <string.h>

#include "dm_core.h"

/* Keyboard input is deliberately limited to printable ASCII. Linux account
   names use this character set, and applying the same constraint to passwords
   means every cursor index is also a complete character boundary. */
int dm_focus_is_field(DmFocus focus) {
    return focus == DM_FOCUS_USERNAME || focus == DM_FOCUS_PASSWORD;
}

static int field_index(DmFocus focus) {
    return dm_focus_is_field(focus) ? (int)focus : -1;
}

static char *field_text(DmCore *core, int field) {
    return field == DM_FIELD_USERNAME ? core->username : core->password;
}

static int *field_length(DmCore *core, int field) {
    return field == DM_FIELD_USERNAME ? &core->username_length
                                      : &core->password_length;
}

static int field_capacity(int field) {
    return field == DM_FIELD_USERNAME ? DM_MAX_USERNAME : DM_MAX_PASSWORD;
}

static void clear_message(DmCore *core) {
    core->message[0] = '\0';
    core->message_is_error = 0;
}

static void selection_bounds(const DmCore *core, int field, int length,
                             int *start, int *end) {
    int anchor = core->field_selection_anchor[field];
    int cursor = core->field_cursor[field];
    if (anchor < 0 || anchor == cursor) {
        *start = cursor;
        *end = cursor;
    } else if (anchor < cursor) {
        *start = anchor;
        *end = cursor;
    } else {
        *start = cursor;
        *end = anchor;
    }
    if (*start < 0) *start = 0;
    if (*start > length) *start = length;
    if (*end < *start) *end = *start;
    if (*end > length) *end = length;
}

static void replace_range(DmCore *core, int field, int start, int end,
                          const char *insert, int insert_length) {
    char *text = field_text(core, field);
    int *length = field_length(core, field);
    int tail_length = *length - end;
    memmove(text + start + insert_length, text + end, (size_t)tail_length + 1);
    if (insert_length > 0) {
        memcpy(text + start, insert, (size_t)insert_length);
    }
    *length = *length - (end - start) + insert_length;
    core->field_cursor[field] = start + insert_length;
    core->field_selection_anchor[field] = core->field_cursor[field];
}

void dm_form_type(DmCore *core, char c) {
    unsigned char character = (unsigned char)c;
    int field = field_index(core->focus);
    if (field < 0 || character < 0x20 || character > 0x7e) return;

    int length = *field_length(core, field);
    int start, end;
    selection_bounds(core, field, length, &start, &end);
    if (length - (end - start) + 1 >= field_capacity(field)) return;

    replace_range(core, field, start, end, &c, 1);
    clear_message(core);
}

void dm_form_backspace(DmCore *core) {
    int field = field_index(core->focus);
    if (field < 0) return;

    int length = *field_length(core, field);
    int start, end;
    selection_bounds(core, field, length, &start, &end);
    if (start == end && start > 0) start--;
    if (start != end) replace_range(core, field, start, end, NULL, 0);
    clear_message(core);
}

void dm_form_delete(DmCore *core) {
    int field = field_index(core->focus);
    if (field < 0) return;

    int length = *field_length(core, field);
    int start, end;
    selection_bounds(core, field, length, &start, &end);
    if (start == end && end < length) end++;
    if (start != end) replace_range(core, field, start, end, NULL, 0);
    clear_message(core);
}

static int is_word_character(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static void move_cursor(DmCore *core, int field, int position) {
    int length = *field_length(core, field);
    if (position < 0) position = 0;
    if (position > length) position = length;
    core->field_cursor[field] = position;
    core->field_selection_anchor[field] = position;
}

void dm_form_cursor_left(DmCore *core, int by_word) {
    int field = field_index(core->focus);
    if (field < 0) return;
    int position = core->field_cursor[field];
    if (core->field_selection_anchor[field] != position) {
        int start, end;
        selection_bounds(core, field, *field_length(core, field), &start, &end);
        move_cursor(core, field, start);
        return;
    }

    char *text = field_text(core, field);
    if (by_word) {
        while (position > 0 && !is_word_character((unsigned char)text[position - 1])) {
            position--;
        }
        while (position > 0 && is_word_character((unsigned char)text[position - 1])) {
            position--;
        }
    } else if (position > 0) {
        position--;
    }
    move_cursor(core, field, position);
}

void dm_form_cursor_right(DmCore *core, int by_word) {
    int field = field_index(core->focus);
    if (field < 0) return;
    int position = core->field_cursor[field];
    if (core->field_selection_anchor[field] != position) {
        int start, end;
        selection_bounds(core, field, *field_length(core, field), &start, &end);
        move_cursor(core, field, end);
        return;
    }

    char *text = field_text(core, field);
    int length = *field_length(core, field);
    if (by_word) {
        while (position < length && is_word_character((unsigned char)text[position])) {
            position++;
        }
        while (position < length && !is_word_character((unsigned char)text[position])) {
            position++;
        }
    } else if (position < length) {
        position++;
    }
    move_cursor(core, field, position);
}

void dm_form_cursor_home(DmCore *core) {
    int field = field_index(core->focus);
    if (field >= 0) move_cursor(core, field, 0);
}

void dm_form_cursor_end(DmCore *core) {
    int field = field_index(core->focus);
    if (field >= 0) move_cursor(core, field, *field_length(core, field));
}

void dm_form_select_all(DmCore *core) {
    int field = field_index(core->focus);
    if (field < 0) return;
    core->field_selection_anchor[field] = 0;
    core->field_cursor[field] = *field_length(core, field);
}

void dm_form_clear_username(DmCore *core) {
    memset(core->username, 0, sizeof(core->username));
    core->username_length = 0;
    core->field_cursor[DM_FIELD_USERNAME] = 0;
    core->field_selection_anchor[DM_FIELD_USERNAME] = 0;
}

int dm_form_copy_display_text(const DmCore *core, DmField field,
                              char *out, size_t capacity) {
    if (!out || capacity == 0 || field < 0 || field >= DM_FIELD_COUNT) {
        return -1;
    }
    int length = field == DM_FIELD_USERNAME ? core->username_length
                                            : core->password_length;
    if ((size_t)length >= capacity) return -1;

    if (field == DM_FIELD_PASSWORD) {
        memset(out, '*', (size_t)length);
    } else if (length > 0) {
        memcpy(out, core->username, (size_t)length);
    }
    out[length] = '\0';
    return length;
}

DmFocus dm_form_focus_next(DmCore *core) {
    core->focus = (DmFocus)((core->focus + 1) % DM_FOCUS_COUNT);
    return core->focus;
}

DmFocus dm_form_focus_prev(DmCore *core) {
    core->focus = (DmFocus)((core->focus + DM_FOCUS_COUNT - 1) % DM_FOCUS_COUNT);
    return core->focus;
}

void dm_form_set_message(DmCore *core, const char *message, int is_error) {
    if (!message) {
        clear_message(core);
        return;
    }
    snprintf(core->message, sizeof(core->message), "%s", message);
    core->message_is_error = is_error;
}

void dm_form_clear_password(DmCore *core) {
    memset(core->password, 0, sizeof(core->password));
    core->password_length = 0;
    core->field_cursor[DM_FIELD_PASSWORD] = 0;
    core->field_selection_anchor[DM_FIELD_PASSWORD] = 0;
}
