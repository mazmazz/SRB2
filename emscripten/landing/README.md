# SRB2 for Web

To install, just drop the contents of this zipfile into a web server.
No other services required, just HTTP.

The HTML file may include a Google Analytics tag. If you wish, you may
search the file for "Google Analytics" and erase the tag.

## Bandwidth Usage

On first run, the server sends around 90 MB of game files to the client.
This includes the core wadfiles and a small selection of music. The
game progressively downloads more music as the player advances.

## Updates

When you use `emscripten-packaging.py`, the script generates a
versioning string to push auto-updates. On every page load, the client
will check for this version string. If it finds that the version is new,
then the page will update on next load.

Game files are checked for updates on every run. The packaging script
generates MD5 hashes for each game file. If the client finds that the
hash is different, then the file is re-downloaded.

## Storage

The game files are stored in the client's IndexedDB store. In Chrome,
you may find these files under Developer Tools > Application.

The shell is capable of deploying multiple SRB2 versions. In cases where
one versions does not have many files changed from a previous versions,
the new release will recall the unchanged files from the previous release.
This saves on bandwidth and the client's disk space.

## Caveats

Low disk-space conditions are not tested. Offline conditions are not
tested. Error-handling is not well-developed.

## Links

* [GitHub repository](https://github.com/mazmazz/SRB2-emscripten)
    * [Getting started](https://github.com/mazmazz/SRB2-emscripten/tree/emscripten-new/emscripten)
    * [Build script](https://github.com/mazmazz/SRB2-emscripten/tree/emscripten-new/emscripten/build-sample.sh)
* [GitHub issues](https://github.com/mazmazz/SRB2-emscripten/issues)
* [GitHub releases](https://github.com/mazmazz/SRB2-emscripten/releases)
