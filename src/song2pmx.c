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
 *     Notes are entered with a capital letter.
 *
 *     Octaves are entered with a digit and stay set until changed. Mid C is
 *     octave 4.
 *
 *     Durations are entered with:
 *       - 'w' whole note
 *       - 'h' half note
 *       - 'q' quarter note
 *       - 'e' eighth note
 *       - 's' sixteenth note
 *       - 't' sets to 2/3rds the current duration
 *       - '.' sets to 1.5 times the current duration
 *     Durations stay set until changed.
 *
 *     The '(' character is used for tie, placed before the note to be extended.
 *
 *     `music.meter_top', `music.meter_bottom' is set with: "M3/4", "M4/4", etc.
 *
 *     Sharp and flat are done with '#' or 'b'.
 *
 *     The variable `music.stacatto_factor' can be set to a range from 0.0 to
 *     1.0.
 *
 *     The variable `music.tempo' is quarter-notes per second. It defaults to
 *     2.5 and gets faster when bigger.
 *
 * Something important to note about the 't' and '.' durations. Terry documented
 * them (in his `Play' function) as "sets to ... the current duration". In
 * practise, when generating songs with `GodSongStr', they only affect 3 and 1
 * notes respectively. This makes sense, since they correspond to a "triplet"
 * and "dot", respectively.
 *
 * Note format for PMX:
 *
 *     [<paren-open>]<note>[<basic-time-value><octave><dots><accidental><paren-close>]<space>
 *
 * Where [...] is used to denote optional. Here's a list of possible values for
 * some of those fields.
 *
 *     <note>:
 *       - a-g: Note in the current octave
 *     <basic-time-value>:
 *       - 9: double-whole note
 *       - 0: whole note
 *       - 2: half note
 *       - 4: quarter note
 *       - 8: eighth note
 *       - 1: sixteenth note
 *       - 3: thirty-second (unused)
 *       - 6: sixty-fourth (unused)
 *     <dots>:
 *       - d: dot, adds 50% of the original note's duration
 *       - dd: double dot, adds 75% of the original note's duration (unused)
 *     <accidental>:
 *       - f: flat, pitch is half step lower until the next bar line
 *       - n: natural, used to cancel flats or sharp for the specified note
 *       - s: sharp, pitch is half step higher until the next bar line
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * TempleOS duration specifiers. They set the current note duration.
 */
enum EDurationSpecifiers {
    DURATION_WHOLE     = 'w',
    DURATION_HALF      = 'h',
    DURATION_QUARTER   = 'q',
    DURATION_EIGHTH    = 'e',
    DURATION_SIXTEENTH = 's',
};

/*
 * TempleOS duration modifiers. They modify (rather than set) the current note
 * duration.
 */
enum EDurationModifiers {
    MODIFIER_TRIPLET = 't',
    MODIFIER_DOT     = '.',
};

/*
 * TempleOS accidentals. They increase or lower the note pitch.
 */
enum EAccidentals {
    ACCIDENTAL_SHARP = '#',
    ACCIDENTAL_FLAT  = 'b',
};

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
           c == DURATION_SIXTEENTH;
}

/*
 * Convert a duration in decimal format to PMX format. Other duration modifiers
 * (such as "triplet" and "dot") are handled in `get_duration_modifier'. See
 * also PMX Manual, Section 2.2.1 Notes.
 */
static const char* get_pmx_duration(char c) {
    /* clang-format off */
    switch (c) {
        case DURATION_WHOLE:      return "0";
        case DURATION_HALF:       return "2";
        case DURATION_QUARTER:    return "4";
        case DURATION_EIGHTH:     return "8";
        case DURATION_SIXTEENTH:  return "1";

        default:
            fprintf(stderr, "Invalid TempleOS duration specifier: '%c'.\n", c);
            abort();
    }
    /* clang-format on */
}

/*----------------------------------------------------------------------------*/

/*
 * Is the specified character a TempleOS song duration modifier?
 */
static inline bool is_duration_modifier(char c) {
    return c == MODIFIER_TRIPLET || c == MODIFIER_DOT;
}

/*
 * Return the PMX string corresponding to a TempleOS duration modifier.
 *
 * The returned string should be placed after the octave in the PMX note, and
 * should take precedence over the the `post_octave' member returned by
 * `get_pmx_duration'.
 *
 * NOTE: This function assumes that the TempleOS `DURMOD_TWO_THIRDS' and
 * `DURMOD_1_50' modifiers only affect 3 or 1 note, respectively. This is true
 * according to Terry's `GodSongStr' function, but not necessarily from its
 * documentation. See the topmost comment of this source file.
 */
static const char* get_pmx_duration_modifier(char c) {
    /* clang-format off */
    switch (c) {
        case MODIFIER_TRIPLET: return "x3";
        case MODIFIER_DOT:     return "d";

        default:
            fprintf(stderr, "Invalid TempleOS duration modifier: '%c'.\n", c);
            abort();
    }
    /* clang-format on */
}

/*----------------------------------------------------------------------------*/

/*
 * Is the specified character a TempleOS sharp or flat specifier?
 */
static inline bool is_accidental(char c) {
    return c == ACCIDENTAL_SHARP || c == ACCIDENTAL_FLAT;
}

/*
 * Convert a TempleOS sharp or flat specifier to a valid PMX accidental
 * specifier.
 */
static const char* get_pmx_accidental(char c) {
    /* clang-format off */
    switch (c) {
        case ACCIDENTAL_SHARP: return "s";
        case ACCIDENTAL_FLAT:  return "f";

        default:
            fprintf(stderr, "Invalid TempleOS accidental: '%c'.\n", c);
            abort();
    }
    /* clang-format on */
}

/*----------------------------------------------------------------------------*/

static const char* write_note(FILE* dst, const char* song) {
    if (*song == '\0')
        return NULL;

    static enum {
        TIE_NONE  = 0,
        TIE_CLOSE = 1,
        TIE_OPEN  = 2,
    } tie_status = TIE_NONE;

    /*
     * The octave and duration variables have to be static, because if a
     * TempleOS note doesn't specify one of these values, we need to fall back
     * to the previous one (the value needs to persist accross calls).
     */
    static const char* duration = "";
    static int octave           = 4;

    /*
     * FIXME: In TempleOS songs, if a "triplet" is set with 't', it remains set
     * until a different note length is specified.
     */
    const char* duration_modifier = "";
    const char* accidental        = "";

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
            fprintf(dst,
                    "m%d/%d/%d/%d ",
                    g_meter_top,
                    g_meter_bottom,
                    g_meter_top,
                    g_meter_bottom);

            /* TODO: Shouldn't we increase `song' here? */
        } else if (isdigit(*song)) {
            octave = *song - '0';
            song++;
        } else if (is_duration_specifier(*song)) {
            duration = get_pmx_duration(*song);
            song++;
        } else if (is_duration_modifier(*song)) {
            duration_modifier = get_pmx_duration_modifier(*song);
            song++;
        } else if (is_accidental(*song)) {
            accidental = get_pmx_accidental(*song);
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

    /* Print the PMX note */
    if (tie_status == TIE_OPEN)
        fprintf(dst, "( ");
    fprintf(dst,
            "%c%s%d%s%s",
            note,
            duration,
            octave,
            duration_modifier,
            accidental);
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
    fprintf(dst,
            "%d %d %d %d ",
            g_meter_top,
            g_meter_bottom,
            g_meter_top,
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
