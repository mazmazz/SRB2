# SONIC ROBO BLAST 2
#-----------------------------------------------------------------------------
# Copyright (C) 1998-2000 by DooM Legacy Team.
# Copyright (C) 1999-2020 by Sonic Team Junior.
#
# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
#-----------------------------------------------------------------------------
#/ \file  enumtables.ps
#/ \brief Generate enumeration string tables for dehacked.

$path = "."
if ($args.Count -gt 0)
{
    $path = $args[0]
}

$enumtables = [IO.File]::ReadAllText("$PSScriptRoot\enumtables.txt")

# bash can't match whitespaces, so each // ENUMTABLES
# directive cannot be indented.
$enumgen_line = '^\s*// ENUMTABLES '

$do_generic = $false
$prefix_in = ""
$prefix_out = ""

$do_actions = $false

$entry_line = ""
$entries = $null

[System.IO.File]::ReadLines("$path\info.h") | ForEach-Object {
    if ($_ -match $enumgen_line)
    {
        if ($_ -match "BEGIN")
        {
            if ($_ -match "STATE_LIST")
            {
                $do_generic = $true
                $prefix_in = "S_"
                $prefix_out = "S_"
            }
            elseif ($_ -match "MOBJTYPE_LIST")
            {
                $do_generic = $true
                $prefix_in = "MT_"
                $prefix_out = "MT_"
            }
            elseif ($_ -match "actionpointers")
            {
                $do_actions = $true
            }
        }
        elseif ($_ -match "END")
        {
            # Use TrimEnd on strings to achieve consistency with bash
            if ($_ -match "STATE_LIST")
            {
                $enumtables = $enumtables -replace "// ENUMTABLES SET STATE_LIST", ($entries -join "`n").TrimEnd()
            }
            elseif ($_ -match "MOBJTYPE_LIST")
            {
                $enumtables = $enumtables -replace "// ENUMTABLES SET MOBJTYPE_LIST", ($entries -join "`n").TrimEnd()
            }
            elseif ($_ -match "actionpointers")
            {
                $enumtables = $enumtables -replace "// ENUMTABLES SET actionpointers", ($entries -join "`n").TrimEnd()
            }

            $do_generic = $false
            $do_actions = $false
            $prefix_in = ""
            $prefix_out = ""
        }

        $entries = New-Object System.Collections.Generic.List[System.String]
    }
    elseif ($do_generic) 
    {
        # Generic behavior: Apply regex ^\tS_([^,]+), --> \t"S_\1",
        # (replace S_ with $prefix)
        # which will quote names, or otherwise copy the line.

        $entry_line = ($_ -replace "^\t$prefix_in([^,]+)", "`t`"$prefix_out`$1`"")
        $entries.Add($entry_line)
    }
    elseif ($do_actions)
    {
        # Action behavior: Extract action name from
        #   void A_SampleName();
        # then format as:
        #   {{A_SampleName},             "A_SAMPLENAME"},

        if ($_ -match "void A_(?<Name>.+)\(\);")
        {
            $entry_line = "`t{{{{A_{0}}}, `"A_{1}`"}}," -f $Matches.Name, $Matches.Name.ToUpper()
            $entries.Add($entry_line)
        }
        else
        {
            $entries.Add($_)
        }
    }
}

# Use TrimEnd on strings to achieve consistency with bash
Set-Content "$path\enumtables.c" $enumtables.TrimEnd()

# Change working directory back to original
Pop-Location
