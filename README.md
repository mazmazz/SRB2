# Sonic Robo Blast 2

[Sonic Robo Blast 2](https://srb2.org/) is a 3D Sonic the Hedgehog fangame based on a modified
version of [Doom Legacy](http://doomlegacy.sourceforge.net/). Over 20 years, this project has enjoyed
thousands of users, along with [press attention](https://www.srb2.org/press/) and a [skilled development team](http://wiki.srb2.org/wiki/Sonic_Team_Junior).

This README details my contributions to this project. I acted as a core programmer, pull request reviewer,
and release manager for v2.1.21 thru v2.1.23. For those releases, I introduced significant new features and
bugfixes within 200k lines of legacy code. I [addressed issues swiftly](https://git.magicalgirl.moe/STJr/SRB2/issues/38)
in response to community feedback.

For repository information, see [STJr/SRB2](https://github.com/STJr/SRB2).

## Accomplishments

### Cross-Platform Releases via Continuous Integration

As release manager, I enabled native macOS and Linux releases by scripting Travis CI to
upload installer builds to FTP and Linux repositories. In addition, I scripted AppVeyor
to build Windows installers automatically.

Previously, the game was released only for Windows, and installers were built by hand.

* [Pull request for macOS and Linux releases](https://git.magicalgirl.moe/KartKrew/Kart-Public/merge_requests/8)
* [Pull request for Windows installers](https://git.magicalgirl.moe/KartKrew/Kart-Public/merge_requests/7)
* [Code files for deployer scripts](https://github.com/STJr/Kart-Public/tree/7806c43ecff710b000cb224b7408e763fdff7a98/deployer)
* [Documentation](http://wiki.srb2.org/wiki/User:Digiku/Cross-platform_deployment)

![](http://wiki.srb2.org/w/images/7/77/DeployerOSXExample.png)

### 64-bit Releases and Benchmarking

I introduced the first 64-bit releases to the game. Previously, only 32-bit was released. I also
constructed a benchmark series to compare 64-bit performance across levels and render modes.

Although anecdotal evidence suggested that heavy levels performed better under 64-bit, a battery
of 1,000+ benchmarks did not demonstrate improved performance. As only a limited number of
environments were tested, the game continues to be released for 64-bit.

* [srb2-benchmark Python code](https://github.com/mazmazz/srb2-benchmark)
* [Intel Core i7 (Skylake) benchmark results](https://1drv.ms/x/s!AsVXpI8zaxfAjbE2dkige4bfiv4DdQ), using Excel Power Query
* [Intel Celeron (Ivy Bridge) benchmark results](https://1drv.ms/x/s!AsVXpI8zaxfAjbE0U1Z89EbYTqF_GQ)

### Fading Platforms

These features allow floating platforms to fade translucently and for colored lights to fade between
different colors. This work involved a team effort in significantly rewriting the legacy lighting code.
An associate cleaned up the old code, while I re-architected his cleanup work to allocate color data dynamically.
Previously, colormaps were assigned to a limited number of entries.

This was merged for the next major version, currently in private development.

* [Documentation and list of branches](https://github.com/mazmazz/SRB2/wiki/Fade-FOF-Test). Click on a branch name to view pull request code.
* [Pull request for colormap code rewrite](https://github.com/STJr/SRB2/compare/SRB2_release_2.1.21...mazmazz:public-colormap-overhaul)
* YouTube demos:
    * [Platform fading](https://www.youtube.com/watch?v=L6h5f3h3B3s)
    * [Global colormap fading](https://youtu.be/xuIWzMJX0_c)
    * [Colormap memory benchmark](https://youtu.be/qYQD5juqlPc)

![srb20011](https://git.magicalgirl.moe/STJr/SRB2Internal/uploads/dbddaf9c7ad5057bfd50cb5d61a750ef/srb20011.gif)
![srb20002](https://git.magicalgirl.moe/STJr/SRB2Internal/uploads/774f6383a98a187ba5b18321afa2608c/srb20002.gif)

### Music Features

SRB2 uses the [SDL Mixer library](https://github.com/SDL-mirror/SDL_mixer), which provides only basic playback features. Due to engine requirements, I implemented [music fading](https://www.youtube.com/watch?v=QSBjUThemKI&list=PLVIEmOPX_YO1sFlGCLZA1Q-ujL30rTM3b&t=0s&index=3) and [jingle switching](https://www.youtube.com/watch?v=1KNtCrbQ-Zo&list=PLVIEmOPX_YO1sFlGCLZA1Q-ujL30rTM3b&t=0s&index=4) by hand. I also implemented [custom MIDI instruments](https://www.youtube.com/watch?v=DMB5qy3dMEU&list=PLVIEmOPX_YO1sFlGCLZA1Q-ujL30rTM3b&t=0s&index=5) by utilizing the [SDL Mixer X library](https://github.com/WohlSoft/SDL-Mixer-X).
I worked with a contributor to improve MOD playback by utilizing the [libopenmpt library](https://github.com/OpenMPT/openmpt).

This work also involved a major rewrite of legacy code. I refactored the game's music code to eliminate
an obsolete duality between MIDI and digital music playback. This resulted in a new, simpler API for music playback.

* [Documentation and list of branches](https://github.com/mazmazz/SRB2/wiki/MusicPlus-Test). Click on a branch name to view pull request code.
* [Pull request for music code refactoring](https://git.magicalgirl.moe/STJr/SRB2/merge_requests/278)
* [Watch this YouTube playlist to see the features in action](https://www.youtube.com/watch?v=DMB5qy3dMEU&index=4&list=PLVIEmOPX_YO1sFlGCLZA1Q-ujL30rTM3b).

### YouTube Level Tutorial

I directed and narrated a tutorial series on building NiGHTS levels, which are the most complicated
type of level to create for the game.

* [Watch this YouTube playlist](https://www.youtube.com/watch?v=BnQLd8gxEUM&index=2&list=PLVIEmOPX_YO2IhFaUJapT4zAsGdUkJK8Q&t=0s).

## Links

* [Pull requests authored by me](https://git.magicalgirl.moe/STJr/SRB2/merge_requests?scope=all&utf8=%E2%9C%93&state=merged&author_username=digiku)
