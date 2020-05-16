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

# bash can't match whitespaces, so each // ENUMTABLES
# directive cannot be indented.
$enumgen_line = '^\s*// ENUMTABLES '

$global:state = [PSCustomObject]@{
    enumtables = [IO.File]::ReadAllText("$PSScriptRoot\enumtables.txt")

    do_generic = $false
    prefix_in = ""
    prefix_out = ""

    do_actions = $false

    entry_line = ""
    entries = $null
}

function Read-Line {
    param( [string]$line )
    if ($line -match $enumgen_line)
    {
        if ($line -match "BEGIN")
        {
            if ($line -match "MOBJFLAG_LIST")
            {
                $global:state.do_generic = $true
                $global:state.prefix_in = "MF_"
                $global:state.prefix_out = ""
            }
            elseif ($line -match "STATE_LIST")
            {
                $global:state.do_generic = $true
                $global:state.prefix_in = "S_"
                $global:state.prefix_out = "S_"
            }
            elseif ($line -match "MOBJTYPE_LIST")
            {
                $global:state.do_generic = $true
                $global:state.prefix_in = "MT_"
                $global:state.prefix_out = "MT_"
            }
            elseif ($line -match "actionpointers")
            {
                $global:state.do_actions = $true
            }
        }
        elseif ($line -match "END")
        {
            # Use TrimEnd on strings to achieve consistency with bash
            if ($line -match "END (?<Name>.*)\s*")
            {
                $global:state.enumtables = $global:state.enumtables -replace "// ENUMTABLES SET $($Matches.Name)", ($global:state.entries -join "`n").TrimEnd()
            }

            $global:state.do_generic = $false
            $global:state.do_actions = $false
            $global:state.prefix_in = ""
            $global:state.prefix_out = ""
        }

        $global:state.entries = New-Object System.Collections.Generic.List[System.String]
    }
    elseif ($global:state.do_generic)
    {
        # Generic behavior: Apply regex ^\tS_([^ \=,]+)[ \=,] --> \t"S_\1",
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

        if ($line -match "^\t$($global:state.prefix_in)(?<Name>[^ \=,]+?)[ \=,]")
        {
            $global:state.entry_line = "`t`"{0}{1}`"," -f $global:state.prefix_out, $Matches.Name
            $global:state.entries.Add($global:state.entry_line)
        }
        else
        {
            $global:state.entries.Add($line)
        }
    }
    elseif ($global:state.do_actions)
    {
        # Action behavior: Extract action name from
        #   void A_SampleName();
        # then format as:
        #   {{A_SampleName},             "A_SAMPLENAME"},

        if ($line -match "void A_(?<Name>.+)\(\);")
        {
            $global:state.entry_line = "`t{{{{A_{0}}}, `"A_{1}`"}}," -f $Matches.Name, $Matches.Name.ToUpper()
            $global:state.entries.Add($global:state.entry_line)
        }
        else
        {
            $global:state.entries.Add($line)
        }
    }
}

[System.IO.File]::ReadLines("$path\info.h") | ForEach-Object {
    Read-Line -line $_
}
[System.IO.File]::ReadLines("$path\p_mobj.h") | ForEach-Object {
    Read-Line -line $_
}

# Use TrimEnd on strings to achieve consistency with bash
Set-Content "$path\enumtables.c" $global:state.enumtables.TrimEnd()

# Change working directory back to original
Pop-Location
