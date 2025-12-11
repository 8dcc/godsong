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
 * Note format for LilyPond:
 *
 *     <note>[<accidental><octave><basic-time-value><dots><tie-indicator>]<space>
 *
 * Where [...] is used to denote optional. Here's a list of possible values for
 * some of those fields.
 *
 *     <note>:
 *       - a-g: Note name.
 *     <accidental>:
 *       - es: flat, pitch is half step lower until the next bar line
 *       - <none>: natural, used to cancel flats or sharp for the specified note
 *       - is: sharp, pitch is half step higher until the next bar line
 *     <octave>:
 *       - ,,: Octave 1
 *       - ,: Octave 2
 *       - <empty>: Octave 3
 *       - ': Octave 4
 *       - '': Octave 5
 *       - ''': Octave 6
 *     <basic-time-value>:
 *       - 1: whole note
 *       - 2: half note
 *       - 4: quarter note
 *       - 8: eighth note
 *       - 16: sixteenth note
 *       - 32: thirty-second (unused)
 *       - 64: sixty-fourth (unused)
 *     <dots>:
 *       - .: dot, adds 50% of the original note's duration
 *       - ..: double dot, adds 75% of the original note's duration (unused)
 *     <tie-indicator>:
 *       - <none>: No tie
 *       - ~: Tie with the next note.
 *
 * Slurs (ties) can also be noted with parentheses, but the closing one must be
 * before the final note.
 *
 * Triplets are noted with "\tuplet 3/2 { ... }".
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
    GODSONG_DURATION_WHOLE     = 'w',
    GODSONG_DURATION_HALF      = 'h',
    GODSONG_DURATION_QUARTER   = 'q',
    GODSONG_DURATION_EIGHTH    = 'e',
    GODSONG_DURATION_SIXTEENTH = 's',
};

/*
 * TempleOS duration modifiers. They modify (rather than set) the current note
 * duration.
 */
enum EDurationModifiers {
    GODSONG_MODIFIER_TRIPLET = 't',
    GODSONG_MODIFIER_DOT     = '.',
};

/*
 * TempleOS accidentals. They increase or lower the note pitch.
 */
enum EAccidentals {
    GODSONG_ACCIDENTAL_SHARP = '#',
    GODSONG_ACCIDENTAL_FLAT  = 'b',
};

/*
 * Top and bottom meter values. Correspond to TempleOS' `music.meter_top' and
 * `music.meter_bottom' variables.
 */
static int g_meter_top    = 4;
static int g_meter_bottom = 4;

/*----------------------------------------------------------------------------*/

/*
 * Read the contents of a file into an allocated buffer, and return it.
 */
static char* read_godsong(FILE* fp) {
    size_t str_pos = 0;
    size_t str_sz  = 0;
    char* str      = NULL;

    for (;;) {
        const int c = fgetc(fp);
        if (c == EOF)
            break;

        /* Non-newline spaces don't have meaning in TempleOS songs */
        if (c != '\n' && isspace(c))
            continue;

        /* If the string is not large enough, reallocate it */
        if (str_pos + 1 >= str_sz) {
            str_sz += 100;
            char* new_str = realloc(str, str_sz);
            if (new_str == NULL) {
                free(str);
                return NULL;
            }
            str = new_str;
        }

        str[str_pos++] = c;
    }

    str[str_pos] = '\0';
    return str;
}

/*----------------------------------------------------------------------------*/

/*
 * Is the specified character a TempleOS song duration specifier?
 */
static inline bool is_duration_specifier(char c) {
    return c == GODSONG_DURATION_WHOLE || c == GODSONG_DURATION_WHOLE ||
           c == GODSONG_DURATION_HALF || c == GODSONG_DURATION_QUARTER ||
           c == GODSONG_DURATION_EIGHTH || c == GODSONG_DURATION_SIXTEENTH;
}

/*
 * Convert a duration in decimal format to LilyPond format. Other duration
 * modifiers (such as "triplet" and "dot") are handled in
 * `get_duration_modifier'.
 */
static const char* get_lilypond_duration(char c) {
    switch (c) {
        case GODSONG_DURATION_WHOLE:
            return "1";

        case GODSONG_DURATION_HALF:
            return "2";

        case GODSONG_DURATION_QUARTER:
            return "4";

        case GODSONG_DURATION_EIGHTH:
            return "8";

        case GODSONG_DURATION_SIXTEENTH:
            return "16";

        default:
            fprintf(stderr, "Invalid TempleOS duration specifier: '%c'.\n", c);
            abort();
    }
}

/*----------------------------------------------------------------------------*/

/*
 * Is the specified character a TempleOS sharp or flat specifier?
 */
static inline bool is_accidental(char c) {
    return c == GODSONG_ACCIDENTAL_SHARP || c == GODSONG_ACCIDENTAL_FLAT;
}

/*
 * Convert a TempleOS sharp or flat specifier to a valid LilyPond accidental
 * specifier.
 */
static const char* get_lilypond_accidental(char c) {
    switch (c) {
        case GODSONG_ACCIDENTAL_SHARP:
            return "is";

        case GODSONG_ACCIDENTAL_FLAT:
            return "es";

        default:
            fprintf(stderr, "Invalid TempleOS accidental: '%c'.\n", c);
            abort();
    }
}

/*----------------------------------------------------------------------------*/

static inline const char* get_lilypond_octave(int octave) {
    switch (octave) {
        case 1:
            return ",,";

        case 2:
            return ",";

        case 3:
            return "";

        case 4:
            return "\'";

        case 5:
            return "\'\'";

        case 6:
            return "\'\'\'";

        default:
            fprintf(stderr, "Invalid TempleOS octave: %d.\n", octave);
            abort();
    }
}

/*----------------------------------------------------------------------------*/

static const char* write_note(FILE* dst, const char* song) {
    if (*song == '\0')
        return NULL;

    /*
     * Triplet status must persist accross calls. A triplet starts with
     * 'GODSONG_MODIFIER_TRIPLET', and ends after 3 notes.
     */
    static bool in_triplet      = false;
    static int notes_in_triplet = 0;

    /*
     * The octave and duration variables have to be static, because if a
     * TempleOS note doesn't specify one of these values, we need to fall back
     * to the previous one (the value needs to persist accross calls).
     */
    static const char* duration = "";
    static int octave           = 4;

    /* These are non-static variables that only affect a single note */
    const char* tie        = "";
    const char* dots       = "";
    const char* accidental = "";

    for (;;) {
        if (*song == '(') {
            tie = "~";
            song++;
        } else if (*song == 'M') { /* Meter specifier */
            song++;
            if (isdigit(*song))
                g_meter_top = *song++ - '0';
            if (*song == '/')
                song++;
            if (isdigit(*song))
                g_meter_bottom = *song++ - '0';

            /* FIXME: Print the new meter here */
            fprintf(dst,
                    "m%d/%d/%d/%d ",
                    g_meter_top,
                    g_meter_bottom,
                    g_meter_top,
                    g_meter_bottom);
        } else if (*song == GODSONG_MODIFIER_DOT) {
            dots = ".";
            song++;
        } else if (*song == GODSONG_MODIFIER_TRIPLET) {
            in_triplet = true;
            song++;
        } else if (isdigit(*song)) {
            octave = *song - '0';
            song++;
        } else if (is_duration_specifier(*song)) {
            duration = get_lilypond_duration(*song);
            song++;
        } else if (is_accidental(*song)) {
            accidental = get_lilypond_accidental(*song);
            song++;
        } else {
            break;
        }
    }

    if (*song < 'A' || *song > 'G') {
        fprintf(stderr, "Warning: Invalid note: '%c' (%#x).\n", *song, *song);
        return NULL;
    }

    /* Actual note. Expressed as lowercase in LilyPond syntax */
    const char note = tolower(*song++);

    /* If we are in a triplet, count the note we are about to print */
    if (in_triplet)
        notes_in_triplet++;

    /* Print the LilyPond note */
    if (in_triplet && notes_in_triplet == 1)
        fprintf(dst, "\\tuplet 3/2 { ");
    fprintf(dst,
            "%c%s%s%s%s%s",
            note,
            accidental,
            get_lilypond_octave(octave),
            duration,
            dots,
            tie);
    if (in_triplet && notes_in_triplet == 3)
        fprintf(dst, "}");
    fputc(' ', dst);

    /* Check if the note is the 3rd and last note in the triplet. Otherwise, */
    if (in_triplet && notes_in_triplet == 3) {
        in_triplet       = false;
        notes_in_triplet = 0;
    }

    return song;
}

static void write_lilypond_header(FILE* dst) {
    fprintf(dst,
            "\\version \"2.24.4\"\n"
            "{\n");
}

static void write_lilypond_footer(FILE* dst) {
    fprintf(dst, "}\n");
}

/*----------------------------------------------------------------------------*/

int main(void) {
    FILE* src = stdin;
    FILE* dst = stdout;

    /* Read the song into an allocated string */
    char* song = read_godsong(src);
    if (song == NULL) {
        fprintf(stderr, "Could not read TempleOS song.\n");
        return 1;
    }

    const char* next_note = song;

    /* Write the LilyPond header and body */
    write_lilypond_header(dst);
    while (next_note != NULL) {
        next_note = write_note(dst, next_note);

        /*
         * Move to the next line
         * TODO: Does this always move to the next staff?
         */
        while (next_note != NULL && *next_note == '\n') {
            fputc('\n', dst);
            next_note++;
        }
    }
    write_lilypond_footer(dst);

    free(song);
    return 0;
}
