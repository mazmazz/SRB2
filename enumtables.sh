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

do_generic=0
prefix_in=""
prefix_out=""

do_actions=0

entry_line=""
entries=""

while IFS= read -r line; do
    if [[ "$line" =~ $enumgen_line ]]; then
        if [[ "$line" == *"BEGIN"* ]]; then
            echo Begin $line
            if [[ "$line" == *"STATE_LIST"* ]]; then
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
            echo End $line
            # Perform regex after all entries are collected
            # during the generic run.
            if [[ "$do_generic" == "1" ]]; then
                entries="`echo -e \"$entries\" | sed -r \"s/^\t$prefix_in([^,]+),/\t\\\"$prefix_out\1\\\",/\"`"
            fi

            if [[ "$line" == *"STATE_LIST"* ]]; then
                enumtables="${enumtables/\/\/ ENUMTABLES SET STATE_LIST/$entries}"
            elif [[ "$line" == *"MOBJTYPE_LIST"* ]]; then
                enumtables="${enumtables/\/\/ ENUMTABLES SET MOBJTYPE_LIST/$entries}"
            elif [[ "$line" == *"actionpointers"* ]]; then
                entries="`echo -e \"$entries\"`"
                enumtables="${enumtables/\/\/ ENUMTABLES SET actionpointers/$entries}"
            fi

            do_generic=0
            do_actions=0
            prefix_in=""
            prefix_out=""
        fi
        entry_line=""
        entries=""
    elif [[ "$do_generic" == "1" ]]; then
        # Generic behavior: Apply regex ^\tS_([^,]+), --> \t"S_\1",
        # (replace S_ with $prefix)
        # which will quote names, or otherwise copy the line.
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
done < "$path/info.h"

echo "$enumtables" > "$path/enumtables.c"
