RedGem 3D Engine for Linux (x86_64) - experimental

Run the editor:

  bin/redgem                    the project hub: create a game or open one
  bin/redgem games/platformer   open a demo; press Play, move with WASD, jump with Space

Needs a 64-bit Linux desktop with OpenGL 3.3, a processor with AVX2 and glibc 2.39 or newer
(Ubuntu 24.04 and later). Nothing is installed: delete the folder to remove it.

Also here:

  bin/redgem-player   what a built game is a copy of
  bin/redgem-relay    the meeting point for multiplayer games over the internet
  bin/redgem-server   runs a game on a server, for players to join (no display needed)
  bin/redgem-test     runs a game's tests
  bin/runtimes/       the players for Windows and Android games

Android builds also need the Android SDK and a JDK. The manual says how:
https://carlosdanielpohlod.github.io/redgem-site/manual/

RedGem is MIT licensed (LICENSE). It is built on open source projects, listed with their
licenses in THIRD_PARTY_LICENSES.md.
