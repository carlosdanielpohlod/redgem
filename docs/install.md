---
description: "Installing RedGem on Linux, Windows and macOS, or building it from source."
---

# Install

RedGem is experimental. The editor is developed on Linux; there are Windows and macOS downloads
too.
Pick one of the ways below.

## Download (easiest)

| System | Download |
|--------|----------|
| Linux (x86_64) | [redgem-linux-x86_64.tar.gz](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-linux-x86_64.tar.gz) |
| Windows (x86_64) | [redgem-windows-x86_64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-windows-x86_64.zip) |
| macOS (Apple Silicon) | [redgem-macos-arm64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-macos-arm64.zip) |
| macOS (Intel) | [redgem-macos-x86_64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-macos-x86_64.zip) |

Unpack it anywhere and run `bin/redgem` (Linux), `redgem.exe` (Windows) or open `RedGem.app`
(macOS). Nothing is installed: delete the folder to remove it. The demos are in its `games/` folder.

- **Linux** needs a desktop with OpenGL 3.3, a processor with AVX2 and glibc 2.39 or newer
  (Ubuntu 24.04 and later). It builds games for Linux, Windows and Android.
- **Windows** is compiled from Linux and has only been tried under Wine, not on a real Windows
  machine yet. It builds games for Windows.
- **macOS** needs macOS 11 or newer. It is built and tested on GitHub's Mac machines (macOS 14 on
  Apple Silicon, macOS 15 on Intel), which are virtual machines with no graphics card OpenGL can
  use: there the editor opens, edits, plays and saves a game drawn by Apple's software renderer.
  It has not been tried on a real Mac yet. It builds games for the Mac only.
  The app is not signed by a registered Apple developer nor notarized, so macOS refuses it the
  first time: right-click `RedGem.app` and choose *Open* (on macOS 15 and later: try once, then
  *System Settings > Privacy & Security > Open Anyway*), or run
  `xattr -dr com.apple.quarantine RedGem.app` in the unpacked folder.

Wherever this manual says `./build/src/redgem ...`, write `bin/redgem ...` (or `redgem.exe ...`,
or `RedGem.app/Contents/MacOS/redgem ...` on macOS).

## With Docker

You need [Docker](https://docs.docker.com/engine/install/) and a desktop session using X11.

```bash
git clone https://github.com/carlosdanielpohlod/redgem.git
cd redgem
tools/docker/editor.sh
```

The first run builds the image, which takes a few minutes. After that the editor opens in seconds.

- Games you create are saved in `~/RedGemGames` on your machine.
- The demos are in the `games/` folder of the checkout: `tools/docker/editor.sh games/platformer`.
- The script takes the same arguments as `redgem` itself, so wherever this manual says
  `./build/src/redgem ...`, write `tools/docker/editor.sh ...`. Paths must be inside
  `~/RedGemGames` or the checkout's `games/` folder: the container sees nothing else.
- The container has no sound.

## From source

You need a C++17 compiler, CMake 3.24 or newer, Ninja, Ruby with rake, and the X11 and OpenGL
development files. On Ubuntu or Debian:

```bash
sudo apt install build-essential cmake ninja-build git ca-certificates xz-utils ruby rake bison \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libasound2-dev
```

On macOS: the Xcode command line tools (`xcode-select --install`), then
`brew install cmake ninja` and a Ruby with rake (the CI builds with Ruby 3.3).

Then:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/src/redgem
```

CMake downloads the libraries RedGem is built on at fixed versions. The first build takes a few
minutes.

## Check that it works

`./build/src/redgem` (or `tools/docker/editor.sh`) opens the **project hub**. Choose
*Platformer* under *New game*, give it a name, and the editor opens with a level you can play
right away: press the Play button, move with WASD, jump with Space.

`./build/src/redgem --version` says which version you have (`RedGem 0.4.0`, say): give it when
you report a problem. (`redgem --help` lists everything else the command line does.)

## Blender (optional)

You can make whole games without it. Install [Blender](https://www.blender.org) 4.x when you want
to model your own objects or build levels there: see [Levels, lighting and Blender](levels.md).
