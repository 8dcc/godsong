#!/usr/bin/env bash
#
# Copyright 2025 8dcc. All Rights Reserved.
#
# This file is part of godsong.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <https://www.gnu.org/licenses/>.
set -e

# URL of the fallback '.sf3' file, for playing the MIDI file.
SF3_URL='https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3'

# If this setting is enabled, and 'lilypond' generates a PDF file, it is opened
# before playing.
OPEN_PDF=true

# ------------------------------------------------------------------------------

if [ $# -ne 1 ]; then
    echo "Usage: $(basename "$0") INPUT.ly" 1>&2
    exit 1
fi

assert_cmd() {
    if [ ! "$(command -v "$1")" ]; then
        echo "$(basename "$0"): The '$1' command is not installed." 1>&2
        exit 1
    fi
}

assert_cmd 'lilypond'
assert_cmd 'curl'
assert_cmd 'fluidsynth'
[ "$OPEN_PDF" == true ] && assert_cmd 'xdg-open'

# ------------------------------------------------------------------------------

input_file="$1"

# Ensure the input ends with '.ly'.
case "$input_file" in
    *.ly) ;;
    *) input_file="${input_file}.ly" ;;
esac

midi_file="${input_file%.ly}.midi"
pdf_file="${input_file%.ly}.pdf"

# ------------------------------------------------------------------------------

# Create temporary directory for common files accross script calls, if it
# doesn't exist.
common_tmpdir='/tmp/play_lilypond_midi_common'
if [ ! -d "$common_tmpdir" ]; then
    echo "$(basename "$0"): Creating common temporary directory at '${common_tmpdir}'."
    mkdir -p "$common_tmpdir"
fi

# Create temporary directory for intermediate files.
olddir="$(pwd)"
tmpdir="$(mktemp --directory --tmpdir 'play_lilypond_midi_XXXXX')"
cp "$input_file" "$tmpdir"
cd "$tmpdir" || exit 1

# Convert the '.ly' file to '.midi' (and perhaps to '.pdf' as well).
echo "$(basename "$0"): Converting '.ly' to '.midi'..."
lilypond "$input_file" 1> "${tmpdir}/lilypond.stdout" 2> "${tmpdir}/lilypond.stderr"
if [ ! -f "$midi_file" ]; then
    echo "$(basename "$0"): LilyPond did not generate the expected MIDI file (${midi_file}). Aborting." 1>&2
    exit 1
fi

# If the '.sf3' file is not present, download it to the common temporary
# direcyory, so it persist accross script calls.
if [ ! -f "${common_tmpdir}/MuseScore_General.sf3" ]; then
    echo "$(basename "$0"): Could not find '.sf3' file, downloading..." 1>&2
    curl -o "${common_tmpdir}/MuseScore_General.sf3" "$SF3_URL"
fi

# Check if the user wants to open the PDF file, and if it was generated.
if [ "$OPEN_PDF" == true ]; then
    if [ -f "$pdf_file" ]; then
        echo "$(basename "$0"): Opening PDF file (${pdf_file})..."
        xdg-open "$pdf_file"
    else
        echo "$(basename "$0"): LilyPond did not generate the expected PDF file (${pdf_file}). Ignoring." 1>&2
    fi
fi

# Play the MIDI file using 'fluidsynth'.
echo "$(basename "$0"): Playing MIDI file..."
fluidsynth -ni "${common_tmpdir}/MuseScore_General.sf3" "$midi_file" 1> "${tmpdir}/fluidsynth.stdout" 2> "${tmpdir}/fluidsynth.stderr"
echo "$(basename "$0"): Done. Intermediate files saved in '${tmpdir}'."
