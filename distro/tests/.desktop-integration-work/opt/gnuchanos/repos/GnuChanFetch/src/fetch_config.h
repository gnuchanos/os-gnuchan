/*
 * fetch_config.h — the settings GnuChanFetch is built from.
 *
 * GnuChanFetch is configured by a plain Key = value file. Older static call
 * syntax is parsed without execution for compatibility. This header is the
 * small, fixed set of values a fetch program can be asked for.
 *
 * Everything here is plain data. The file is read by fetch_parser.c, walked
 * by fetch_config.c, and drawn and printed by GnuChanFetch.c and fetch_image.c.
 * Nothing in this header knows about a terminal or a picture.
 */
#ifndef GNUCHANFETCH_CONFIG_H
#define GNUCHANFETCH_CONFIG_H

/* A written value: a colour, a path, a field name. Long enough for any of them
   and shared by the parser and the config so one length governs both. */
#define FETCH_TEXT_LENGTH 256

/* The most fields a file may ask to be shown, and the most colours its swatch
   line may hold. Both are far above what a person writes, and both exist so a
   malformed file cannot make the walker write past its arrays. */
#define FETCH_MAX_FIELDS  32
#define FETCH_MAX_COLOURS 16

typedef struct FetchConfig {
    /* The picture shown to the left of the information, as written: a path, in
       which a leading "~" is the home directory. Empty means no picture, and
       the information is printed on its own. */
    char image[FETCH_TEXT_LENGTH];

    /* How tall the picture is drawn, in text ROWS, and the columns of blank
       left between it and the information. The picture's WIDTH is never written
       here: it follows from the picture's own shape, so the picture is never
       stretched. */
    int image_rows;
    int gap;

    /* The colour the picture is drawn on where it is transparent, so a logo
       with a see-through background reads as sitting on the terminal rather
       than on a black rectangle. */
    char image_background[FETCH_TEXT_LENGTH];

    /* The fields to print, in the order they are printed. A name that is not
       one this program has is reported and skipped; see fetch_info.c for the
       set of names. */
    char fields[FETCH_MAX_FIELDS][FETCH_TEXT_LENGTH];
    int field_count;

    /* The swatch of colours drawn under the title, as many as are written. */
    char colours[FETCH_MAX_COLOURS][FETCH_TEXT_LENGTH];
    int colour_count;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[FETCH_TEXT_LENGTH * 2];
} FetchConfig;

/* The settings a machine with no file gets: the logo in the usual place and
   the fields a fetch program is expected to show. */
void fetch_config_defaults(FetchConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which
   case config is left at its defaults and `error` says why. */
int fetch_config_load(FetchConfig *config, const char *path);

/* Read the settings from the standard place, or the defaults when there is no
   file. Always leaves a usable config. Returns 0 when a file was read. */
int fetch_config_load_default(FetchConfig *config);

/* Where settings are looked for: $XDG_CONFIG_HOME/GnuChanFetch/GnuChanFetch.conf,
   or ~/.config/GnuChanFetch/GnuChanFetch.conf. */
char *fetch_config_path(char *buffer, unsigned int size);

#endif /* GNUCHANFETCH_CONFIG_H */
