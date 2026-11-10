#include <stdio.h>
#include <string.h>

#include "../src/notif_dbus.c"

static DBusMessage *make_image_message(int first_field_is_string,
                                      int width, int height, int row_stride,
                                      dbus_bool_t has_alpha,
                                      int bits_per_sample, int channels,
                                      const unsigned char *pixels,
                                      int pixel_count) {
    DBusMessage *message = dbus_message_new_signal(
        "/test", "org.example.Test", "Image");
    if (!message) {
        return NULL;
    }

    DBusMessageIter root;
    DBusMessageIter variant;
    DBusMessageIter structure;
    DBusMessageIter array;
    dbus_message_iter_init_append(message, &root);
    const char *signature = first_field_is_string
                                ? "(siibiiay)"
                                : "(iiibiiay)";
    if (!dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT,
                                           signature, &variant) ||
        !dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL,
                                          &structure)) {
        dbus_message_unref(message);
        return NULL;
    }

    if (first_field_is_string) {
        const char *text = "bad";
        dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &text);
    } else {
        dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &width);
    }
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &height);
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &row_stride);
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_BOOLEAN, &has_alpha);
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32,
                                   &bits_per_sample);
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &channels);
    if (!dbus_message_iter_open_container(&structure, DBUS_TYPE_ARRAY,
                                          DBUS_TYPE_BYTE_AS_STRING, &array)) {
        dbus_message_unref(message);
        return NULL;
    }
    const unsigned char *data = pixels;
    dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &data,
                                         pixel_count);
    dbus_message_iter_close_container(&structure, &array);
    dbus_message_iter_close_container(&variant, &structure);
    dbus_message_iter_close_container(&root, &variant);
    return message;
}

static int parse_image(NotifRequest *request, int first_field_is_string,
                       int width, int height, int row_stride,
                       dbus_bool_t has_alpha, int bits_per_sample,
                       int channels, const unsigned char *pixels,
                       int pixel_count) {
    DBusMessage *message = make_image_message(
        first_field_is_string, width, height, row_stride, has_alpha,
        bits_per_sample, channels, pixels, pixel_count);
    if (!message) {
        return 0;
    }

    DBusMessageIter root;
    DBusMessageIter variant;
    int parsed = 0;
    if (dbus_message_iter_init(message, &root) &&
        dbus_message_iter_get_arg_type(&root) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(&root, &variant);
        parsed = read_image_data(&variant, request);
    }
    dbus_message_unref(message);
    return parsed;
}

static int check(int condition, const char *description) {
    if (!condition) {
        fprintf(stderr, "failed: %s\n", description);
        return 0;
    }
    return 1;
}

int main(void) {
    const unsigned char rgba[] = { 0x10, 0x20, 0x30, 0x7f };
    NotifRequest request;
    memset(&request, 0, sizeof(request));

    if (!check(parse_image(&request, 0, 1, 1, 4, TRUE, 8, 4,
                           rgba, sizeof(rgba)),
               "accepts a complete 8-bit RGBA image") ||
        !check(request.image_card_count == 1 &&
                   request.image_cards[0] == 0x7f102030L,
               "preserves image channels when alpha is declared")) {
        notif_request_free(&request);
        return 1;
    }
    notif_request_free(&request);

    memset(&request, 0, sizeof(request));
    if (!check(parse_image(&request, 0, 1, 1, 4, FALSE, 8, 4,
                           rgba, sizeof(rgba)),
               "accepts a complete opaque four-channel image") ||
        !check(request.image_cards[0] == 0xff102030L,
               "ignores the fourth channel when alpha is not declared")) {
        notif_request_free(&request);
        return 1;
    }
    notif_request_free(&request);

    memset(&request, 0, sizeof(request));
    if (!check(!parse_image(&request, 0, 2, 1, 8, TRUE, 8, 4,
                            rgba, sizeof(rgba)),
               "rejects a payload shorter than its declared stride")) {
        notif_request_free(&request);
        return 1;
    }
    if (!check(!parse_image(&request, 1, 1, 1, 4, TRUE, 8, 4,
                            rgba, sizeof(rgba)),
               "rejects a struct field with the wrong D-Bus type")) {
        notif_request_free(&request);
        return 1;
    }
    if (!check(!parse_image(&request, 0, 1, 1, 8, TRUE, 16, 4,
                            rgba, sizeof(rgba)),
               "rejects unsupported sample widths")) {
        notif_request_free(&request);
        return 1;
    }
    if (!check(!parse_image(&request, 0, 513, 1, 2052, TRUE, 8, 4,
                            rgba, sizeof(rgba)),
               "rejects images exceeding the renderer's side limit")) {
        notif_request_free(&request);
        return 1;
    }

    puts("notification image-data tests passed");
    return 0;
}
