# Emscripten Shell

The emscripten shell manages versioning of game data and provides an interface to run the
game and handle system events.

The shell is designed to be an entirely flat-file implementation. No server-side scripting is
required to host the shell.

# Quick Start

Refer to `build-sample.sh` to compile the game binary and package it for web.

# File Structure

```
- index.html           -- Shell
- version-program.txt  -- Text file with the default version string. Triggers an upgrade notification if
                          the browser's stored version string is different from the server's.
- version-shell.txt    -- Text file with the shell verison string. Triggers an auto-reload on
                          standalone installations ("Add to Home Screen")
- assets/              -- Shell assets, such as background and Apple splash images
- data/                -- Game data
-   {version}/         -- Subfolder to store the game version's binary and assets.
-     _BASE            -- Text file with the name of a version to use assets from (see BASE, below).
-     _FULLINSTALL     -- Text file listing every file to download for a "full install" IN ADDITION TO
                          _INSTALL. Not used, currently.
-     _INSTALL         -- Text file listing the files to download on first run of the game.
-     _PERSISTENT      -- Text file listing the files which will always stay in the in-memory file
                          system during gameplay.
-     _REQUIRED        -- Text file listing the files that must be downloaded for the game to run
-     _STARTUP         -- Text file listing the files that will be loaded to the in-memory file system
                          on game start. _PERSISTENT is added to this list on runtime.
-     {wadfile}        -- Wadfile
-     {wadfile}.md5    -- Text file with the MD5 hash of a wadfile. This must exist for the shell
                          to recognize the wadfile.
-     srb2.wasm        -- Game binary
-     srb2.wasm.md5    -- ...
-     srb2.js          -- Emscripten support code, stored per version
-     srb2.js.md5      -- ...
```

# BASE Version for Game Assets

A game version may specify a BASE version in which that version's game data is used. This way,
only the changed files need to be in the current version's folder.

For example, if you are releasing 2.2.5, the only changed file might be `patch.pk3`. In this case,
in the _BASE file of 2.2.5, you can specify "2.2.4". In the 2.2.5 folder, you may place `patch.pk3`
and `patch.pk3.md5` in order to use the more recent wadfile. All other wadfiles which don't exist
in the 2.2.5 folder will instead be pulled from the 2.2.4 folder.

# FWAD and File Lumps

Emscripten supports an FWAD feature in which a wadfile contains only metadata of the lumps,
and the lumps themselves are stored as files in a subfolder. This allows for partial downloading
of a wad package, which saves on bandwidth.

For example, if you specify `music.dta` for FWAD conversion, then the package script will
generate `data/{version}/music.dta` for lump metadata and output the lumps as
`data/{version}/_music.dta/{index}_{lumpname}`. The index refers to the lump order in the wadfile.

Optionally, you may specify that the lump files are not named by index. Internally, this is marked
by the file header EWAD. In the `emscripten-package.py` script, if you want to name lump files
by index, you'll use the `--fwad` switch; otherwise, use the `--ewad` switch.

# How Game Assets are Retrieved and Stored on Client-side

When a client does a fresh-install of the game, the files and their MD5 hashes are stored in the
browser's IndexedDB store, under `SRB2_DATA/FILES/data/{version}/{wadfile}`. The list of files
to retrieve are baked in the shell HTML file.

On subsequent runs, the client requests the MD5 hash of each file from the server. If the server's
MD5 hash is different from the client's hash, then the file is re-downloaded.

If a game version uses a wadfile in a BASE version, then only the BASE copy of the wadfile is stored
in the client. The game version will have a marker for its wadfile that points to the BASE copy.

In Chrome Developer Tools, you can see the browser's IndexedDB store under the "Applications" tab
when you are viewing the landing page.

# emscripten-package.py Sample Usage

To build the landing package, use `python emscripten-package.py -h` to see arguments. 

To build from default binary and data directories, and convert music.dta to FWAD with no index naming:

```
python emscripten-package.py 2.2.4 --ewad music.dta --out-zip srb2-web.zip
```

To make a patch release that updates one file from a BASE release, you'll want to build the
BASE version's game assets first, then build the patch release.

The first command skips the landing page and binary files to compile only the game assets. The game
data is pulled from `./2.2.4-data` and copied to the default game asset directory for the version,
`emscripten/landing/data/2.2.4`.

The second command builds a release that successfully routes BASE files to 2.2.4. Binary files are
copied from the default build directory (`bin/emscripten/Release`). The landing page is generated
and 2.2.5-specific assets will go into `emscripten/landing/data/2.2.5`. The output ZIP file will 
contain the assets of both 2.2.4 and 2.2.5 -- whichever is in `emscripten/landing/data` at the time 
of output.

```
python emscripten-package.py 2.2.4 --skip-landing --skip-build --data-dir ./2.2.4-data
--ewad music.dta

python emscripten-package.py 2.2.5 --base-version 2.2.4 --data-dir ./2.2.5-data --out-zip srb2-web.zip
```

Finally, you can make the shell page offer multiple versions for play by using the `--package-versions`
switch. The `--default-package-version` switch tells the shell to select a certain version on first
run. The shell page will display a dropdown through which the user may select what to play.

If only one package version is offered (the default), then the dropdown is hidden.

```
python emscripten-package.py 2.2.5 --package-versions 2.2.4 2.2.5 --default-package-version 2.2.5
--base-version 2.2.4 --data-dir ./2.2.5-data --out-zip srb2-web.zip
```

The version string is arbitrary. For example, to offer a feature build alongside the main release,
you could name a version `2.2.4-softpoly` with a BASE version of `2.2.4`.
