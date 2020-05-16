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

# bash can't match whitespaces, so each // ENUMTABLES
# directive cannot be indented.
enumgen_line="^\s*// ENUMTABLES "
action_match="^\s*void A_(.*)\(\);"
set_match="^\s*// ENUMTABLES END (.*).*"

do_generic=0
prefix_in=""
prefix_out=""

do_actions=0

entry_line=""
entries=""

function process_line () {
    line=$1
    if [[ "$line" =~ $enumgen_line ]]; then
        if [[ "$line" == *"BEGIN"* ]]; then
            if [[ "$line" == *"MOBJFLAG_LIST"* ]]; then
                do_generic=1
                prefix_in="MF_"
                prefix_out=""
            elif [[ "$line" == *"STATE_LIST"* ]]; then
                do_generic=1
                prefix_in="S_"
                prefix_out="S_"
            elif [[ "$line" == *"MOBJTYPE_LIST"* ]]; then
                do_generic=1
                prefix_in="MT_"
                prefix_out="MT_"
            elif [[ "$line" == *"actionpointers"* ]]; then
                do_actions=1
            fi
        elif [[ "$line" == *"END"* ]]; then
            if [[ "$do_generic" == "1" ]]; then
                entries="`echo -e \"$entries\" | sed -r \"s/^\t$prefix_in([^ \=,]*?).*,/\t\\\"$prefix_out\1\\\",/\"`"
            elif [[ "$do_actions" == "1" ]]; then
                entries="`echo -e \"$entries\"`"
            fi

            if [[ "$line" =~ $set_match ]]; then
                name=${BASH_REMATCH[1]}
                enumtables="${enumtables/\/\/ ENUMTABLES SET $name/$entries}"
            fi

            do_generic=0
            do_actions=0
            prefix_in=""
            prefix_out=""
        fi
        entry_line=""
        entries=""
    elif [[ "$do_generic" == "1" ]]; then
        # Generic behavior: Apply regex ^\tS_([^ \=,]*?).*, --> \t"S_\1",
        # which will quote names, or otherwise copy the line.
        # In PowerShell, this discards inline comments on the same line.
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

echo "$enumtables" > "$path/enumtables.c"
