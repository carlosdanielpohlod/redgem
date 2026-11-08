# Release downloads

A release carries four downloads, under these names (the README's
`releases/latest/download/<name>` links depend on them):

| Download | What it holds | Built on |
| --- | --- | --- |
| `redgem-linux-x86_64.tar.gz` | the editor and its tools in `bin/`, with the players for Windows and Android games in `bin/runtimes/windows` and `bin/runtimes/android` | Ubuntu 24.04 |
| `redgem-windows-x86_64.zip` | the editor, the player, the relay and the game server | Ubuntu 24.04, with llvm-mingw |
| `redgem-macos-arm64.zip` | `RedGem.app` for Apple Silicon | macOS 15 (arm64) |
| `redgem-macos-x86_64.zip` | `RedGem.app` for Intel Macs | macOS 15 (Intel) |

Each one also has the demos (platformer, driving, city), `LICENSE`, `THIRD_PARTY_LICENSES.md` and
its `README.txt` (`README-<system>.txt` here).

## How a release is made

The continuous integration builds and packs the downloads (`.github/workflows/release.yml`). The
compilers are pinned: llvm-mingw by `tools/windows/fetch_toolchain.sh`, the Android NDK by
`tools/android/fetch_toolchain.sh`, the runners by their image names.

1. Bump the version in `CMakeLists.txt` and move the `[Unreleased]` part of `CHANGELOG.md` under
   it, on a branch named `release-<version>`, and open its pull request. Its run builds the four
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
  relay's and the server's `--help`, and on a Mac the app's signature). That is all: no editor
  is opened from a download (the Mac's app is opened for a picture, which is not a check), and no
  game is built with one.
- Then the four together: if one is missing, or any job failed, nothing is attached.
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
tools/release/package.sh                 # dist/release: the Linux and Windows downloads
tools/release/check_downloads.sh dist/release redgem-linux-x86_64.tar.gz redgem-windows-x86_64.zip
tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.tar.gz
```

On a Mac, `tools/release/package.sh` packs that Mac's download instead.
