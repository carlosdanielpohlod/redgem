# Install

RedGem runs on Linux. Pick one of the two ways below.

## With Docker (easiest)

You need [Docker](https://docs.docker.com/engine/install/) and a desktop session using X11.

```bash
git clone <repository URL> redgem
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

## Blender (optional)

You can make whole games without it. Install [Blender](https://www.blender.org) 4.x when you want
to model your own objects or build levels there: see [Levels, lighting and Blender](levels.md).
