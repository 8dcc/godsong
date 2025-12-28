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

if [ $# -ne 2 ]; then
    echo "Usage: $(basename "$0") INPUT.pmx OUTPUT.pdf" 1>&2
    exit 1
fi

assert_cmd() {
    if [ ! "$(command -v "$1")" ]; then
        echo "$(basename "$0"): The '$1' command is not installed." 1>&2
        exit 1
    fi
}

assert_cmd 'pmxab'
assert_cmd 'pdftex'
assert_cmd 'musixflx'

# ------------------------------------------------------------------------------

input_file="$1"
output_file="$2"

# Ensure the input ends with '.pmx'.
case "$input_file" in
    *.pmx) ;;
    *) input_file="${input_file}.pmx" ;;
esac

tex_file="${input_file%.pmx}.tex"
pdf_file="${tex_file%.tex}.pdf"

# ------------------------------------------------------------------------------

# Create temporary directory for intermediate files.
olddir="$(pwd)"
tmpdir="$(mktemp --directory --tmpdir 'pmx2pdf_XXXXX')"
cp "$input_file" "$tmpdir"
cd "$tmpdir" || exit 1

# Convert the '.pmx' file to '.tex'.
pmxab "$input_file"
if [ ! -f "$tex_file" ]; then
    echo "$(basename "$0"): Could not find output TeX file after 'pmxab' command." 1>&2
    exit 1
fi

# Convert the '.tex' file to '.pdf'. See Section 1.3 "The three pass system" of
# the MusiXTeX manual.
pdftex "$tex_file"
musixflx "$tex_file"
pdftex "$tex_file"
if [ ! -f "$pdf_file" ]; then
    echo "$(basename "$0"): Could not find output PDF file after MusiXTeX commands." 1>&2
    exit 1
fi

cp "$pdf_file" "${olddir}/${output_file}"
echo "$(basename "$0"): Wrote output to '${output_file}'. Intermediate files saved in '${tmpdir}'."
