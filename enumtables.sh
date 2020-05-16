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

enumgen_line="^\s*// ENUMTABLES "
action_match="^\s*void A_(.*)\(\);"

do_generic=0
prefix=""

do_actions=0

entry_line=""
entries=""
states=""
mobjtypes=""
actions=""

while IFS= read -r line; do
    if [[ "$line" =~ $enumgen_line ]]; then
        # Perform regex after all entries are collected
        # during the generic run.
        if [[ "$do_generic" == "1" ]]; then
            if [[ "$line" == *"END"* ]]; then
                entries="`echo -e \"$entries\" | sed -r \"s/^\t$prefix([^,]+),/\t\\\"$prefix\1\\\",/\"`"
            fi
        fi

        # States
        if [[ "$line" == *"STATE_LIST"* ]]; then
            if [[ "$line" == *"BEGIN"* ]]; then
                do_generic=1
                prefix="S_"
            elif [[ "$line" == *"END"* ]]; then
                do_generic=0
                prefix=""
                states=$entries
            fi
        fi

        # Mobj Types
        if [[ "$line" == *"MOBJTYPE_LIST"* ]]; then
            if [[ "$line" == *"BEGIN"* ]]; then
                do_generic=1
                prefix="MT_"
            elif [[ "$line" == *"END"* ]]; then
                do_generic=0
                prefix=""
                mobjtypes=$entries
            fi
        fi

        # Actions
        if [[ "$line" == *"actionpointers"* ]]; then
            if [[ "$line" == *"BEGIN"* ]]; then
                do_actions=1
            elif [[ "$line" == *"END"* ]]; then
                do_actions=0
                actions="`echo -e \"$entries\"`"
            fi
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

enumtables=`cat $SCRIPTPATH/enumtables.txt`
enumtables="${enumtables/\/\/ ENUMTABLES STATE_LIST/$states}"
enumtables="${enumtables/\/\/ ENUMTABLES MOBJTYPE_LIST/$mobjtypes}"
enumtables="${enumtables/\/\/ ENUMTABLES actionpointers/$actions}"
echo "$enumtables" > "$path/enumtables.c"
