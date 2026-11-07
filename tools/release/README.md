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
   it, on a branch named `release-<version>`, and open its pull request. Its run attaches the
   downloads to a draft release for `v<version>` (made when there is none). Try them.
2. Merge it, and push the tag: `git tag v<version> <merge commit> && git push origin v<version>`.
   The tag's run builds the downloads again from the tag and attaches them to the same draft,
   replacing what the pull request attached. A tag that does not name the version in
   `CMakeLists.txt` (`v<version>`, or a pre-release such as `v<version>-rc1`) fails.
3. On GitHub, write the release's notes and publish the draft. Nothing publishes it on its own.

Each run checks every download before it keeps it: what it holds
(`tools/release/check_downloads.sh`), and its programs run on its own system
(`tools/release/smoke_test.sh`: the editor's `--version`, the relay's and the server's `--help`,
and on a Mac the app's signature). Then the four are checked together: if one is missing, or any
job failed, nothing is attached to a release. A published release is never changed
(`tools/release/attach_to_draft.sh` fails instead).

To try the workflow without touching any release, start it by hand (Actions > Release > Run
workflow) with `dry_run` ticked, as it is by default: the downloads are only kept as the run's
artifacts, `redgem-<system>`.

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
