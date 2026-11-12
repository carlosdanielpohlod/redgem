# Release downloads

A release carries five downloads, under these names (the README's
`releases/latest/download/<name>` links depend on them):

| Download | What it holds | Built on |
| --- | --- | --- |
| `redgem-linux-x86_64.tar.gz` | the editor and its tools in `bin/`, with the players for Windows and Android games in `bin/runtimes/windows` and `bin/runtimes/android` | Ubuntu 22.04 (GCC 12, libstdc++ linked in): runs on glibc 2.35 and later |
| `redgem-linux-x86_64.AppImage` | the tarball's files, with `AppRun` (`AppRun.sh` here: the editor, or the program named first), the menu entry and the icon from `branding/` | packed from the tarball's files with appimagetool and its runtime (pinned by `fetch_appimage_tools.sh`) |
| `redgem-windows-x86_64.zip` | the editor, the player, the relay and the game server | Ubuntu 24.04, with llvm-mingw |
| `redgem-macos-arm64.zip` | `RedGem.app` for Apple Silicon | macOS 15 (arm64) |
| `redgem-macos-x86_64.zip` | `RedGem.app` for Intel Macs | macOS 15 (Intel) |

Each one also has the demos (platformer, driving, city), `LICENSE`, `THIRD_PARTY_LICENSES.md` and
its `README.txt` (`README-<system>.txt` here).

Beside them, the players' symbols: each player as it was linked, before it was stripped for the
downloads, which `redgem symbolize` reads a crash report of a game built with it with (it finds
the one of the report's build by its build id). They are never in a download.

| Asset | What it holds |
| --- | --- |
| `redgem-symbols-linux.zip` | `redgem-symbols/linux/redgem-player` (the very one the Linux downloads carry, built in the Ubuntu 22.04 container) |
| `redgem-symbols-windows.zip` | `redgem-symbols/windows/redgem-player.exe` (linked with `--strip-debug`: functions, no lines) |
| `redgem-symbols-android.zip` | `redgem-symbols/android/<arm64-v8a, x86_64>/libredgem.so` |

The Mac's players carry no build id the engine reads yet: no Mac symbols are kept.

The Linux programs are built in an Ubuntu 22.04 container, so that they take from glibc only
what 2.35 has, and need no libstdc++ of the system's. `glibc_floor.sh` measures that (the newest
`GLIBC_` version of their symbols, by `objdump -T`) and fails when it is newer than
`LINUX_GLIBC_FLOOR` in `release_files.sh`, or when a doc says another floor. The release workflow
runs it on the programs as built, and `smoke_test.sh` again on both Linux downloads, with a demo
built for Linux by each, in a bare Ubuntu 22.04 container (glibc 2.35, none of the build's
packages): a game built for Linux is a copy of `bin/redgem-player`, so it runs where the editor
does. No older system can run them: the floor is the build system's own glibc.

## How a release is made

The continuous integration builds and packs the downloads (`.github/workflows/release.yml`). The
compilers are pinned: llvm-mingw by `tools/windows/fetch_toolchain.sh`, the Android NDK by
`tools/android/fetch_toolchain.sh`, the runners by their image names.

1. Bump the version in `CMakeLists.txt` and move the `[Unreleased]` part of `CHANGELOG.md` under
   it, on a branch named `release-<version>`, and open its pull request. Its run builds the five
   downloads from the branch's head and keeps them as the run's artifacts (`redgem-<system>`):
   try them. It touches no release. It fails at once when the branch does not name the version
   in `CMakeLists.txt`, or when that version's release is published already.
2. Merge it, and push the tag: `git tag v<version> <merge commit> && git push origin v<version>`.
   The tag's run builds the downloads from the tag and attaches them to the tag's draft release,
   which it makes when there is none (a pre-release, for a tag such as `v<version>-rc1`). Only a
   tag's run attaches anything, so the draft only holds downloads built from its tag. A tag that
   does not name the version in `CMakeLists.txt`, or whose release is published, fails before
   anything is built. Running it again (re-running the run) replaces the draft's downloads.
3. On GitHub, write the release's notes and publish the draft. Nothing publishes it on its own.

What a run checks, and what it does not:

- Every download, before it is kept: what it holds (`tools/release/check_downloads.sh`), and its
  programs run on its own system (`tools/release/smoke_test.sh`: the editor's `--version`, the
  relay's and the server's `--help`, and on a Mac the app's signature; for the Linux downloads,
  also `redgem check` and `redgem build --target linux` on a demo and the glibc floor of the
  programs and of the built game, on the runner and again in a bare Ubuntu 22.04 container). That is
  all: no editor window is opened from a download (the Mac's app is opened for a picture, which
  is not a check), and no game is played.
- Then the five together, and the players' symbols: if one is missing, or any job failed, nothing
  is attached.
- Attaching (`tools/release/attach_to_draft.sh`): the new downloads go up under other names
  (`incoming.<name>`), are checked to be all there at the sizes packed, and only then, the release
  checked again to still be a draft, replace the old ones. A failed upload leaves the old ones. A
  published release is never changed. (The checks and the swap are separate calls to GitHub: the
  owner publishing the draft during the seconds of the swap is not guarded against.)

To try the workflow without touching any release, start it by hand (Actions > Release > Run
workflow): a run by hand only keeps the downloads as its artifacts.

## By hand

The same scripts pack the downloads from local builds:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
tools/windows/build_runtime.sh
cmake --build build-windows --target redgem redgem_relay redgem_server
tools/android/build_runtime.sh
tools/release/package.sh                 # dist/release: the Linux and Windows downloads, and the symbols
tools/release/check_downloads.sh dist/release redgem-linux-x86_64.tar.gz \
  redgem-linux-x86_64.AppImage redgem-windows-x86_64.zip \
  redgem-symbols-linux.zip redgem-symbols-windows.zip redgem-symbols-android.zip
tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.tar.gz
tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.AppImage
```

Built so on a recent system, the Linux programs need that system's glibc and libstdc++, so
`smoke_test.sh` (through `glibc_floor.sh`) refuses them: the release's are built as the
workflow's `linux-build` job does.

On a Mac, `tools/release/package.sh` packs that Mac's download instead.
