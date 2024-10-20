/*
 * Copyright 2024 8dcc
 *
 * This file is part of godsong.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * godsong. If not, see <https://www.gnu.org/licenses/>.
 *
 * ============================================================================
 *
 * Paraphrasing Terry's comment on his `Play' function (adding missing stuff):
 *
 *   Notes are entered with a capital letter.
 *
 *   Octaves are entered with a digit and stay set until changed. Mid C is
 *   octave 4.
 *
 *   Durations are entered with:
 *     - 'w' whole note
 *     - 'h' half note
 *     - 'q' quarter note
 *     - 'e' eighth note
 *     - 's' sixteenth note
 *     - 't' sets to 2/3rds the current duration
 *     - '.' sets to 1.5 times the current duration
 *   Durations stay set until changed.
 *
 *   The '(' character is used for tie, placed before the note to be extended.
 *
 *   `music.meter_top', `music.meter_bottom' is set with:
 *   "M3/4"
 *   "M4/4"
 *   etc.
 *
 *   Sharp and flat are done with '#' or 'b'.
 *
 *   The variable `music.stacatto_factor' can be set to a range from 0.0 to 1.0.
 *
 *   The variable `music.tempo' is quarter-notes per second. It defaults to 2.5
 *   and gets faster when bigger.
 *
 * Note format for PMX:
 *
 *   [<paren-open>]<note>[<basic-time-value><octave><dots><accidental><paren-close>]<space>
 *
 * Where [...] is used to denote optional.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * TempleOS duration specifiers.
 */
enum EDurationSpecifiers {
    DURATION_WHOLE      = 'w',
    DURATION_HALF       = 'h',
    DURATION_QUARTER    = 'q',
    DURATION_EIGHTH     = 'e',
    DURATION_SIXTEENTH  = 's',
    DURATION_TWO_THIRDS = 't',
    DURATION_1_50       = '.',
};

/*
 * The last set octave [0..9]. By default 4, middle C to B above.
 */
static int g_octave = 4;

/*
 * The last set note duration. By default "quarter" (1).
 */
static double g_duration = 1.0;

/*
 * Top and bottom meter values. Correspond to TempleOS' `music.meter_top' and
 * `music.meter_bottom' variables.
 */
static int g_meter_top    = 4;
static int g_meter_bottom = 4;

/*----------------------------------------------------------------------------*/

static char* read_song(FILE* fp) {
    size_t str_i  = 0;
    size_t str_sz = 100;
    char* str     = malloc(str_sz);

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c != '\n' && isspace(c))
            continue;

        if (str_i + 1 >= str_sz) {
            str_sz += 100;
            str = realloc(str, str_sz);
        }

        str[str_i++] = c;
    }

    str[str_i] = '\0';
    return str;
}

/*----------------------------------------------------------------------------*/

/*
 * Is the specified character a TempleOS song duration specifier?
 */
static inline bool is_duration_specifier(char c) {
    return c == DURATION_WHOLE || c == DURATION_WHOLE || c == DURATION_HALF ||
           c == DURATION_QUARTER || c == DURATION_EIGHTH ||
           c == DURATION_SIXTEENTH || c == DURATION_TWO_THIRDS ||
           c == DURATION_1_50;
}

/*
 * Return a PMX duration from a TempleOS specifier. See PMX Manual, Section
 * 2.2.1 Notes.
 */
static double duration_from_specifier(double old_duration, char c) {
    /* clang-format off */
    switch (c) {
        case DURATION_WHOLE:      return 4.00;
        case DURATION_HALF:       return 2.00;
        case DURATION_QUARTER:    return 1.00;
        case DURATION_EIGHTH:     return 0.50;
        case DURATION_SIXTEENTH:  return 0.25;
        case DURATION_TWO_THIRDS: return 2.00 * old_duration / 3.0;
        case DURATION_1_50:       return 1.50 * old_duration;

        default:
            fprintf(stderr, "Invalid duration modifier: '%c' (%#x).\n", c, c);
            abort();
    }
    /* clang-format on */
}

/*
 * Convert the decimal duration to PMX format.
 */
static const char* get_pmx_duration(double duration) {
    if (duration >= 12.0) /* Dotted double-whole */
        return "d9";
    if (duration >= 8.00) /* Double-whole */
        return "9";
    if (duration >= 6.00) /* Dotted whole */
        return "d0";
    if (duration >= 4.00) /* Whole */
        return "0";
    if (duration >= 3.00) /* Dotted half */
        return "d2";
    if (duration >= 2.00) /* Half */
        return "2";
    if (duration >= 1.50) /* Dotted quarter */
        return "d4";
    if (duration >= 1.00) /* Quarter */
        return "4";
    if (duration >= 0.75) /* Dotted eighth */
        return "d8";
    if (duration >= 0.50) /* Eighth */
        return "8";
    if (duration >= 0.375) /* Dotted sixteenth */
        return "d1";
    if (duration >= 0.25) /* Sixteenth */
        return "1";

    fprintf(stderr, "Unsupported duration: %f\n", duration);
    abort();
}

/*
 * Is the specified character a TempleOS sharp or flat specifier?
 */
static inline bool is_accidental(char c) {
    return c == '#' || c == 'b';
}

/*
 * Convert a TempleOS sharp or flat specifier to a valid PMX accidental
 * specifier.
 */
static char convert_accidental(char c) {
    /* clang-format off */
    switch (c) {
        case '#': return 's'; /* Sharp */
        case 'b': return 'f'; /* Flat */

        default:
            fprintf(stderr, "Invalid pitch modifier: '%c' (%#x).\n", c, c);
            abort();
    }
    /* clang-format on */
}

static const char* write_note(FILE* dst, const char* song) {
    static enum {
        TIE_NONE  = 0,
        TIE_CLOSE = 1,
        TIE_OPEN  = 2,
    } tie_status = TIE_NONE;

    if (*song == '\0')
        return NULL;

    for (;;) {
        if (*song == '(') {
            tie_status = TIE_OPEN;
            song++;
        } else if (*song == 'M') { /* Meter specifier */
            song++;
            if (isdigit(*song))
                g_meter_top = *song++ - '0';
            if (*song == '/')
                song++;
            if (isdigit(*song))
                g_meter_bottom = *song++ - '0';

            /* Print the new meter here */
            fprintf(dst, "m%d/%d/%d/%d ", g_meter_top, g_meter_bottom,
                    g_meter_top, g_meter_bottom);
        } else if (isdigit(*song)) {
            /* NOTE: Octaves and durations don't need to be global? */
            g_octave = *song - '0';
            song++;
        } else if (is_duration_specifier(*song)) {
            g_duration = duration_from_specifier(g_duration, *song);
            song++;
        } else {
            break;
        }
    }

    if (*song < 'A' || *song > 'G') {
        fprintf(stderr, "Warning: Invalid note: '%c' (%#x).\n", *song, *song);
        return NULL;
    }

    /* Actual note. Expressed as lowercase in PMX syntax */
    const char note = tolower(*song++);

    /* Accidentals like sharp of flat */
    char accidental = 0;
    for (; is_accidental(*song); song++)
        accidental = convert_accidental(*song);

    /* Print the PMX note */
    if (tie_status == TIE_OPEN)
        fprintf(dst, "( ");
    fprintf(dst, "%c%s%d", note, get_pmx_duration(g_duration), g_octave);
    if (accidental != 0)
        fputc(accidental, dst);
    if (tie_status == TIE_CLOSE)
        fprintf(dst, " )");
    fputc(' ', dst);

    /* Go to next tie status: From open to close, and from close to none. */
    if (tie_status > TIE_NONE)
        tie_status--;

    return song;
}

static void write_pmx_header(FILE* dst) {
    /* Staves and instruments: nv, noinst */
    fprintf(dst, "1 1 ");

    /* Meter: mtrnuml, mtrdenl, mtrnmp, mtrdnp */
    fprintf(dst, "%d %d %d %d ", g_meter_top, g_meter_bottom, g_meter_top,
            g_meter_bottom);

    /* xmtrnum0, isig */
    fprintf(dst, "0 0\n");

    /*
     * TODO: Improve.
     * npages, nsyst, musicsize, fracindent
     */
    fprintf(dst, "0 4 20 0\n");

    /* Instrument name: Blank */
    fprintf(dst, "\n");

    /* Clef */
    fprintf(dst, "7\n");

    /* Output path */
    fprintf(dst, "./\n\n");
}

/*----------------------------------------------------------------------------*/

int main(void) {
    FILE* src = stdin;
    FILE* dst = stdout;

    /* Read the song into an allocated string */
    char* song            = read_song(src);
    const char* next_note = song;

    /* Write the PMX header and body */
    write_pmx_header(dst);
    while (next_note != NULL) {
        next_note = write_note(dst, next_note);

        /* Move to the next staff */
        while (next_note != NULL && *next_note == '\n') {
            fprintf(dst, "/\n");
            next_note++;
        }
    }
    fputc('\n', dst);

    free(song);
    return 0;
}
