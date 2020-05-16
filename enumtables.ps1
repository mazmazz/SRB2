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

# bash can't match whitespaces, so each // ENUMTABLE
# directive cannot be indented.
$enumtables_line = '^\s*// ENUMTABLE '

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
    if ($line -match $enumtables_line)
    {
        # // ENUMTABLE BEGIN TABLE_NAME ACTION_NAME ARG1 ARG2
        # [0][1]       [2]   [3]        [4]         [5]  [6]
        $tokens = $line.Trim().Split(" ")

        if ($tokens[2] -match "BEGIN")
        {
            if ($tokens[4] -match "do_generic")
            {
                $global:state.do_generic = $true
                $global:state.prefix_in = If ($tokens.Count -gt 5) { $tokens[5] } Else { "" }
                $global:state.prefix_out = If ($tokens.Count -gt 6) { $tokens[6] } Else { "" }
            }
            elseif ($tokens[4] -match "do_actions")
            {
                $global:state.do_actions = $true
            }
        }
        elseif ($tokens[2] -match "END")
        {
            # Use TrimEnd on strings to achieve consistency with bash
            $global:state.enumtables = $global:state.enumtables -replace "// ENUMTABLE SET $($tokens[3])", ($global:state.entries -join "`n").TrimEnd()

            $global:state.do_generic = $false
            $global:state.do_actions = $false
            $global:state.prefix_in = ""
            $global:state.prefix_out = ""
        }

        $global:state.entries = New-Object System.Collections.Generic.List[System.String]
    }
    elseif ($global:state.do_generic)
    {
        # Generic behavior: Apply regex ^\tS_([^ \=,]+?)[ \=,] --> \t"S_\1",
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

        if ($line -match "^\t$($global:state.prefix_in)(?<Name>[^ \=,]+?)[ \=,]")
        {
            $global:state.entry_line = "`t`"{0}{1}`"," -f $global:state.prefix_out, $Matches.Name.ToUpper()
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
[System.IO.File]::ReadLines("$path\doomstat.h") | ForEach-Object {
    Read-Line -line $_
}
[System.IO.File]::ReadLines("$path\d_player.h") | ForEach-Object {
    Read-Line -line $_
}
[System.IO.File]::ReadLines("$path\m_menu.h") | ForEach-Object {
    Read-Line -line $_
}
[System.IO.File]::ReadLines("$path\st_stuff.h") | ForEach-Object {
    Read-Line -line $_
}

# Use TrimEnd on strings to achieve consistency with bash
Set-Content "$path\enumtables.c" $global:state.enumtables.TrimEnd()

# Change working directory back to original
Pop-Location
