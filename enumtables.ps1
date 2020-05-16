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

$enumgen_line = '^\s*// ENUMTABLES '

$do_generic = $false
$prefix = ""

$do_actions = $false

$entry_line = ""
$entries = $null
$states = $null
$mobjtypes = $null
$actions = $null

[System.IO.File]::ReadLines("$path\info.h") | ForEach-Object {
    if ($_ -match $enumgen_line)
    {
        # States
        if ($_ -match "STATE_LIST")
        {
            if ($_ -match "BEGIN")
            {
                $do_generic = $true
                $prefix = "S_"
            }
            elseif ($_ -match "END")
            {
                $do_generic = $false
                $prefix = ""
                $states = $entries
            }
        }

        # MOBJ Types
        if ($_ -match "MOBJTYPE_LIST")
        {
            if ($_ -match "BEGIN")
            {
                $do_generic = $true
                $prefix = "MT_"
            }
            elseif ($_ -match "END")
            {
                $do_generic = $false
                $prefix = ""
                $mobjtypes = $entries
            }
        }

        # Actions
        if ($_ -match "actionpointers")
        {
            if ($_ -match "BEGIN")
            {
                $do_actions = $true
            }
            elseif ($_ -match "END")
            {
                $do_actions = $false
                $actions = $entries
            }
        }

        $entries = New-Object System.Collections.Generic.List[System.String]
    }
    elseif ($do_generic) 
    {
        # Generic behavior: Apply regex ^\tS_([^,]+), --> \t"S_\1",
        # (replace S_ with $prefix)
        # which will quote names, or otherwise copy the line.

        $entry_line = ($_ -replace "^\t$prefix([^,]+)", "`t`"$prefix`$1`"")
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
$enumtables = [IO.File]::ReadAllText("$PSScriptRoot\enumtables.txt")
$enumtables = $enumtables -replace "// ENUMTABLES STATE_LIST", ($states -join "`n").TrimEnd()
$enumtables = $enumtables -replace "// ENUMTABLES MOBJTYPE_LIST", ($mobjtypes -join "`n").TrimEnd()
$enumtables = $enumtables -replace "// ENUMTABLES actionpointers", ($actions -join "`n").TrimEnd()
Set-Content "$path\enumtables.c" $enumtables.TrimEnd()

# Change working directory back to original
Pop-Location
