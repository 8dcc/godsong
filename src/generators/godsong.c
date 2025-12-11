/*
 * Copyright 2024 8dcc
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
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h> /* time() */

#define LENGTH(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

enum EDurations {
    DUR_4           = 0,
    DUR_8_8         = 1,
    DUR_3_3_3       = 2,
    DUR_16_16_16_16 = 3,
    DUR_8DOT_16     = 4,
    DUR_8_16_16     = 5,
    DUR_16_16_8     = 6,
};

enum EComplexities {
    COMPLEXITY_SIMPLE  = 0,
    COMPLEXITY_NORMAL  = 1,
    COMPLEXITY_COMPLEX = 2,
};

static uint8_t god_simple_songs[]  = { DUR_4, DUR_4, DUR_4, DUR_4, DUR_8_8 };
static uint8_t god_normal_songs[]  = { DUR_4,
                                       DUR_4,
                                       DUR_8_8,
                                       DUR_3_3_3,
                                       DUR_16_16_16_16 };
static uint8_t god_complex_songs[] = {
    DUR_4,     DUR_4,       DUR_8_8,     DUR_8_8,        DUR_8DOT_16,
    DUR_3_3_3, DUR_8_16_16, DUR_16_16_8, DUR_16_16_16_16
};

/*
 * Should we use rests in the current song? Terry sets it to 'false'.
 */
static bool g_use_rests = false;

/*
 * Octave of the current note.
 */
static uint64_t g_octave = 4;

/*
 * Currently effective octave, according to what we have written in the song
 * buffer. Doesn't need to match `octave'.
 */
static uint64_t g_octave_old;

/*----------------------------------------------------------------------------*/

/*
 * Return a random 64-bit number. Unfortunately without the aid of the Holy
 * Spirit.
 */
static uint64_t godbits(int nbits) {
    uint64_t mask = 0;
    while (nbits-- > 0) {
        mask <<= 1;
        mask |= 1;
    }

    return rand() & mask;
}

/*
 * Return the character that should be written to the buffer to represent the
 * specified octave.
 */
static inline char octave2char(int octave) {
    assert(octave >= 0 && octave <= '9');
    return octave + '0';
}

/*
 * Get a note duration with the specified `complexity'.
 *
 * NOTE: The `random' parameter could be removed, since we always pass
 * 'godbits(8)'.
 */
static uint8_t get_duration(int complexity, int random) {
    switch (complexity) {
        case 0:
            return god_simple_songs[random % LENGTH(god_simple_songs)];
        case 1:
            return god_normal_songs[random % LENGTH(god_normal_songs)];
        case 2:
            return god_complex_songs[random % LENGTH(god_complex_songs)];
        default:
            fprintf(stderr, "Invalid complexity.");
            abort();
    }
}

/*
 * Insert a note into `buf' at `buf_pos'.
 */
static void insert_note(char* buf, int* buf_pos, uint64_t random) {
    if (random == 0 && g_use_rests) {
        buf[(*buf_pos)++] = 'R';
        return;
    }

    /*
     * TODO: I am sure the logic behind these conditionals can be improved.
     */
    random /= 2;
    if (random < 3) {
        if (g_octave_old != g_octave) {
            g_octave_old      = g_octave;
            buf[(*buf_pos)++] = octave2char(g_octave_old);
        }
    } else {
        if (g_octave_old != g_octave + 1) {
            g_octave_old      = g_octave + 1;
            buf[(*buf_pos)++] = octave2char(g_octave_old);
        }
    }

    buf[(*buf_pos)++] = (random == 0) ? 'G' : random - 1 + 'A';
}

char* godsong(int len, int complexity) {
    /* Allow six eighths */
    assert(len == 8 || len == 6);

    /* Random seed, used by `godbits' */
    srand(time(NULL));

    int buf_pos = 0;
    char* buf   = calloc(256, sizeof(char));
    if (buf == NULL)
        return NULL;

    /*
     * FIXME: Why does he do this?
     */
    g_octave_old   = g_octave + 1;
    buf[buf_pos++] = octave2char(g_octave_old);
    if (len == 6) {
        buf[buf_pos++] = 'M';
        buf[buf_pos++] = '6';
        buf[buf_pos++] = '/';
        buf[buf_pos++] = '8';
    }

    uint8_t last_duration = -1;
    for (int i = 0; i < len; i++) {
        uint8_t duration = get_duration(complexity, godbits(8));

        switch (duration) {
            case DUR_8_8: {
                if (last_duration != DUR_8_8)
                    buf[buf_pos++] = 'e';

                insert_note(buf, &buf_pos, godbits(4));
                insert_note(buf, &buf_pos, godbits(4));
            } break;

            case DUR_8DOT_16: {
                buf[buf_pos++] = 'e';
                buf[buf_pos++] = '.';
                insert_note(buf, &buf_pos, godbits(4));
                buf[buf_pos++] = 's';
                insert_note(buf, &buf_pos, godbits(4));
                duration = DUR_16_16_16_16;
            } break;

            case DUR_3_3_3: {
                if (last_duration != DUR_3_3_3) {
                    buf[buf_pos++] = 'e';
                    buf[buf_pos++] = 't';
                }

                insert_note(buf, &buf_pos, godbits(4));
                insert_note(buf, &buf_pos, godbits(4));
                insert_note(buf, &buf_pos, godbits(4));
            } break;

            case DUR_8_16_16: {
                if (last_duration != DUR_8_8)
                    buf[buf_pos++] = 'e';

                insert_note(buf, &buf_pos, godbits(4));
                buf[buf_pos++] = 's';
                insert_note(buf, &buf_pos, godbits(4));
                insert_note(buf, &buf_pos, godbits(4));
                duration = DUR_16_16_16_16;
            } break;

            case DUR_16_16_8: {
                if (last_duration != DUR_16_16_16_16)
                    buf[buf_pos++] = 's';

                insert_note(buf, &buf_pos, godbits(4));
                insert_note(buf, &buf_pos, godbits(4));
                buf[buf_pos++] = 'e';
                insert_note(buf, &buf_pos, godbits(4));
                duration = DUR_8_8;
            } break;

            case DUR_16_16_16_16: {
                if (last_duration != DUR_16_16_16_16)
                    buf[buf_pos++] = 's';

                const uint8_t random1 = godbits(4);
                const uint8_t random2 = godbits(4);
                insert_note(buf, &buf_pos, random1);
                insert_note(buf, &buf_pos, random2);
                insert_note(buf, &buf_pos, random1);
                insert_note(buf, &buf_pos, random2);
            } break;

            default: {
                if (last_duration != DUR_4)
                    buf[buf_pos++] = 'q';

                insert_note(buf, &buf_pos, godbits(4));
            } break;
        }

        last_duration = duration;
    }

    return buf;
}

int main(void) {
    FILE* dst = stdout;

    /* Generate the song */
    char* result = godsong(8, COMPLEXITY_SIMPLE);
    if (result == NULL) {
        fprintf(stderr, "Could not generate song.\n");
        return 1;
    }

    /* Print the result */
    fprintf(dst, "%s\n", result);

    free(result);
    return 0;
}
