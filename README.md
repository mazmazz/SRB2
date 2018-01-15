# SRB2 MusicPlus

SRB2 MusicPlus implements Lua functions for scriptable, gameplay-adaptable music. For SRB2 2.1.

## Lua Functions

* `S_PositionMusic(position)` -- set position in milliseconds
* `S_GetPositionMusic()` -- get position in milliseconds
* `S_DigitalPlaying()` -- is current song digital?
* `S_MidiPlaying()` -- is current song a MIDI?
* `S_MusicPlaying()` -- is any music playing (either MIDI or digital)?
* `S_MusicPaused()` -- is music paused?
* `S_MusicPause()` -- pause music
* `S_MusicResume()` -- resume music
* `S_MusicName()` -- get current music name
* `S_MusicExists(name[, checkMIDI[, checkDigi]])` -- check if musicname exists. Set `checkMIDI` or `checkDigi` flags to enable checking `D_` or `O_` lumps, respectively. Both flags default to true.

## Lua Hooks

* `ChangeMusic function(oldname, newname, flags, looping)` -- Get notified of music changes. 

Return true to not change music; false or nil to continue changing music; or return a string to override to another music name. Return an empty string to stop playing music. 

Optionally, this can output up to 3 values -- value #2 is the music flags to override (nil preserves the new flags), and value #3 is a boolean whether to loop or not (nil preserves the new value).

## Console Commands

TUNES is changed to allow seeking position.

* `TUNES [name] [track] [speed] [position milliseconds]`

Note that TUNES -show may not show the correct track if it was changed by Lua -- use MUSICNAME in this case.

Use the following commands by adding test_musicplus.wad:

* `MUSICHELP`
* `MUSICPLAYING`
* `MUSICPOS`
* `MUSICNAME`
* `MUSICEXISTS name [checkMIDI bool] [checkDigi bool]`

## Notes

Adding `test_underwatermusic.wad` demonstrates music-changing underwater if you warp to `MAP07`.

For a timing demo, add `test_musicplus.wad` and `PLAYDEMO test_musicplus-demo.lmp`.

### Technical Details

MusicPlus implements for SDL2, SDL1.2, and FMOD (`srb2dd.exe`). Compared to PLUSC, MusicPlus aims to provide a more feature-complete and stable solution for scriptable music.

## Links

* Repo: https://github.com/mazmazz/SRB2/tree/musicplus
* Releases: https://github.com/mazmazz/SRB2/releases
* Issues: https://github.com/mazmazz/SRB2/issues

### SRB2 PLUSC Commits

* [S_MusicVolume, S_FadeOutMusic, S_ChangeMusicFadeIn](https://github.com/yellowtd/SRB2-PLUS/commit/4d9b9ab74fd38ff218c914f757b09f12b0fcb9f6)
* [SetMusicPosition, GetMusicPosition, SDL callback](https://github.com/yellowtd/SRB2-PLUS/commit/4741ae718a24186ede9109159df90c280ccd9e80)

# Sonic Robo Blast 2

[![Build status](https://ci.appveyor.com/api/projects/status/399d4hcw9yy7hg2y?svg=true)](https://ci.appveyor.com/project/STJr/srb2)
[![Build status](https://travis-ci.org/STJr/SRB2.svg?branch=master)](https://travis-ci.org/STJr/SRB2)
[![CircleCI](https://circleci.com/gh/STJr/SRB2/tree/master.svg?style=svg)](https://circleci.com/gh/STJr/SRB2/tree/master)

[Sonic Robo Blast 2](https://srb2.org/) is a 3D Sonic the Hedgehog fangame based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

## Dependencies
- NASM (x86 builds only)
- SDL2 (Linux/OS X only)
- SDL2-Mixer (Linux/OS X only)
- libupnp (Linux/OS X only)
- libgme (Linux/OS X only)

Warning: 64-bit builds are not netgame compatible with 32-bit builds. Use at your own risk.

## Compiling

See [SRB2 Wiki/Source code compiling](http://wiki.srb2.org/wiki/Source_code_compiling)

## Disclaimer
Sonic Team Junior is in no way affiliated with SEGA or Sonic Team. We do not claim ownership of any of SEGA's intellectual property used in SRB2.
