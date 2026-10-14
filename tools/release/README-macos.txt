RedGem 3D Engine for macOS - experimental

Open RedGem.app: the project hub opens, where you create a game or open one. To open a demo,
type its folder under "Open a game folder" (for example ~/Downloads/redgem/games/platformer)
and press Open, or from a terminal, in this folder:

  RedGem.app/Contents/MacOS/redgem games/platformer

Press Play, move with WASD, jump with Space.

The app is not signed by a registered Apple developer nor notarized by Apple (that needs a paid
Apple account), so macOS refuses to open it the first time. Either:

  - right-click RedGem.app and choose Open, then Open again (on macOS 15 and later: try to open
    it once, then System Settings > Privacy & Security > Open Anyway), or
  - in a terminal, in this folder: xattr -dr com.apple.quarantine RedGem.app

This download has been built and tested on GitHub's Mac machines (macOS 14 on Apple Silicon,
macOS 15 on Intel), which are virtual machines without a graphics card for OpenGL: there it ran
on Apple's software renderer. It has not been tried on a real Mac yet: please report what
breaks. It needs macOS 11 or newer. It builds games for the Mac; Windows, Linux and Android
builds are made from the Linux download.

Also in RedGem.app/Contents/MacOS/:

  redgem-player   what a built game is a copy of
  redgem-relay    the meeting point for multiplayer games over the internet
  redgem-test     runs a game's tests

The manual: https://carlosdanielpohlod.github.io/redgem-site/manual/

RedGem is MIT licensed (LICENSE). It is built on open source projects, listed with their
licenses in THIRD_PARTY_LICENSES.md.
