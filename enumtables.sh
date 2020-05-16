#!/bin/bash

# SONIC ROBO BLAST 2
#-----------------------------------------------------------------------------
# Copyright (C) 1998-2000 by DooM Legacy Team.
# Copyright (C) 1999-2020 by Sonic Team Junior.
#
# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
#-----------------------------------------------------------------------------
#/ \file  enumtables.sh
#/ \brief Generate enumeration string tables for dehacked.

path="."
if [ x"$1" != x ]; then
	path="$1"
fi

SCRIPT=`realpath $0`
SCRIPTPATH=`dirname $SCRIPT`

enumtables=`cat $SCRIPTPATH/enumtables.txt`

# bash can't match whitespaces, so each // ENUMTABLE
# directive cannot be indented.
enumtables_line="^\s*// ENUMTABLE "
action_match="^\s*void A_(.*)\(\);"

do_generic=0
prefix_in=""
prefix_out=""

do_actions=0

entry_line=""
entries=""

function process_line () {
    line=$1
    if [[ "$line" =~ $enumtables_line ]]; then
        # // ENUMTABLE BEGIN TABLE_NAME ACTION_NAME ARG1 ARG2
        # [0][1]       [2]   [3]        [4]         [5]  [6]
        # Trim trailing newlines to prevent match bugs
        tokens=($line)
        verb_name="`echo ${tokens[2]} | tr -d '\r'`"
        table_name="`echo ${tokens[3]} | tr -d '\r'`"
        behavior_name="`echo ${tokens[4]} | tr -d '\r'`"

        if [[ "$verb_name" == "BEGIN" ]]; then
            if [[ "$behavior_name" == "do_generic" ]]; then
                do_generic=1
                # if arg is empty, should default to blank string
                prefix_in="`echo ${tokens[5]} | tr -d '\r'`"
                prefix_out="`echo ${tokens[6]} | tr -d '\r'`"
            elif [[ "$behavior_name" == "do_actions" ]]; then
                do_actions=1
            fi
        elif [[ "$verb_name" == "END" ]]; then
            if [[ "$do_generic" == "1" ]]; then
                entries="`echo -e \"$entries\" | sed -r \"s/^\t$prefix_in([^ \=,]*?).*[ \=\/,].*/\t\\\"$prefix_out\U\1\\\",/\"`"
            elif [[ "$do_actions" == "1" ]]; then
                entries="`echo -e \"$entries\"`"
            fi

            enumtables="${enumtables/\/\/ ENUMTABLE SET $table_name/$entries}"

            do_generic=0
            do_actions=0
            prefix_in=""
            prefix_out=""
        fi
        entry_line=""
        entries=""
    elif [[ "$do_generic" == "1" ]]; then
        # Generic behavior: Apply regex ^\tS_([^ \=,]*?).*[ \=\/,].*, --> \t"S_\1",
        # which will quote names, or otherwise copy the line.
        # This discards inline comments on the same line.
        #
        # Works:
        #   S_SAMPLENAME,
        #   S_SAMPLENAME ,
        #   S_SAMPLENAME=VALUE,
        #   S_SAMPLENAME = VALUE,
        # Does not work: lines which do not have a comma, or have tabs
        #   S_SAMPLENAME
        #   S_SAMPLENAME\t,
        #
        # Apply the regex at the END instruction (see above), due to
        # performance reasons.

        entries="${entries}${line}\n"
    elif [[ "$do_actions" == "1" ]]; then
        # Action behavior: Extract action name from
        #   void A_SampleName();
        # then format as:
        #   {{A_SampleName},             "A_SAMPLENAME"},

        if [[ "$line" =~ $action_match ]]; then
            name=${BASH_REMATCH[1]}
            name_upper=`echo $name | tr [a-z] [A-Z]`
            entry_line="{{A_$name}, \"A_$name_upper\"},"
            entries="${entries}\t${entry_line}\n"
        else
            entries="${entries}${line}\n"
        fi
    fi
}

while IFS= read -r line; do
    process_line "$line"
done < "$path/info.h"

while IFS= read -r line; do
    process_line "$line"
done < "$path/p_mobj.h"

while IFS= read -r line; do
    process_line "$line"
done < "$path/doomstat.h"

while IFS= read -r line; do
    process_line "$line"
done < "$path/d_player.h"

while IFS= read -r line; do
    process_line "$line"
done < "$path/m_menu.h"

while IFS= read -r line; do
    process_line "$line"
done < "$path/st_stuff.h"

echo "$enumtables" > "$path/enumtables.c"
