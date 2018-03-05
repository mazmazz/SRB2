# SRB2 MusicPlus

SRB2 MusicPlus implements Lua functions for scriptable, gameplay-adaptable music. For SRB2 2.1.

## Lua Functions

* `S_SetMusicPosition(position)` -- set position in milliseconds
* `S_GetMusicPosition()` -- get position in milliseconds
* `S_DigitalPlaying()` -- is current song digital?
* `S_MidiPlaying()` -- is current song a MIDI?
* `S_MusicPlaying()` -- is any music playing (either MIDI or digital)?
* `S_MusicPaused()` -- is music paused?
* `S_MusicPause()` -- pause music
* `S_MusicResume()` -- resume music
* `S_MusicName()` -- get current music name
* `S_MusicExists(name[, checkMIDI[, checkDigi]])` -- check if musicname exists. Set `checkMIDI` or `checkDigi` flags to enable checking D_ or O_ lumps, respectively. Both flags default to true.
* `P_PlayJingle(jingletype)` -- play predefined jingles (see `jingles_t` constants in d_player.h)
* `P_PlayJingleMusic(musname, looping, delay, fadein, resetpremus)` -- play custom jingle that fires the `MusicJingle` hook

All functions take `player_t` as their last parameter (except `S_ChangeMusic`, where it's the third parameter; and `S_MusicExists`, which is not strictly a sound function.)

## Lua Hooks

* `MusicChange function(oldname, newname, flags, looping)` -- Get notified of music changes. 
    * Output 1: true to not change music; false/nil to continue changing magic; string to override to another music name
    * Output 2: Music flags to override
    * Output 3: Boolean whether to loop or not
* `MusicJingle function(jingletype, newname, looping, delay, fadein, jinglereset)` -- Get notified when a predefined jingle plays.
    * Output 1: true to not change music; false/nil to continue changing magic; string to override to another music name
    * Output 2: Boolean whether to loop or not
    * Output 3: Delay milliseconds to use for post-jingle restore
    * Output 4: Fade-in milliseconds to use for post-jingle restore
    * Output 5: Boolean to reset post-jingle music at position 0 and not fade-in.
* `MusicRestore function(newname, newpos, delay, fadein, flags, looping)` -- Get notified when the music is restored from a jingle.
    * Output 1: true to not change music; false/nil to continue changing magic; string to override to another music name
    * Output 2: Position milliseconds to seek to
    * Output 3: Delay milliseconds to use for post-jingle restore
    * Output 4: Fade-in milliseconds to use for post-jingle restore
    * Output 5: Music flags to override
    * Output 6: Boolean whether to loop or not

All outputs are optional.

## Console Commands

TUNES is changed to allow seeking position.

* `TUNES [name] [track] [speed] [position milliseconds]`

Note that `TUNES -show` may not show the correct track if it was changed by Lua -- use `MUSICNAME` in this case.

Use the following commands by adding test_musicplus.wad:

* `MUSICHELP`
* `MUSICPLAYING`
* `MUSICPOS`
* `MUSICNAME`
* `MUSICEXISTS name [checkMIDI bool] [checkDigi bool]`

## Notes

Adding `test_underwatermusic.wad` demonstrates music-changing underwater if you warp to MAP07.

For a timing demo, add `test_musicplus.wad` and `PLAYDEMO test_musicplus-demo.lmp`.

### Network Safety

To be net-safe with these functions, iterate through every player and pass each `player_t` object to the desired function. The function, if passed a non-local player, will bypass the internal music calls and return `nil`, in order to remain network safe.

`S_MusicExists` does not take a `player_t` parameter because it's not strictly a sound function -- it only checks for lumps.

It is unknown if the Music hooks are network safe. Again, if making any music calls, pass each `player_t` object to the call to attempt network safety.

### Technical Details

MusicPlus implements for SDL2, SDL1.2, and FMOD (srb2dd.exe). Compared to PLUSC, MusicPlus aims to provide a more feature-complete and stable solution for scriptable music.

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
