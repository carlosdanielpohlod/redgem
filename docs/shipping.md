---
description: "Building a standalone game for Linux, Windows, macOS and Android, and recording videos."
---

# Building and sharing

**Build** in the editor's toolbar, or from a terminal:

```bash
./build/src/redgem build my_game dist/my_game
```

This makes a folder with an executable named after your game (`dist/my_game/my_game`) and
`game.rgpak`, one file that holds all of the game's files (see [The game's files](#the-games-files)
below). Whoever receives the folder runs the executable: they do not need RedGem, Ruby or Blender
installed.

That executable is the **player**: a copy of `redgem-player`, a small program that plays the game
packed in `game.rgpak` beside it and does nothing else. The editor, the project hub and the other
tools you make the game with are not in it. A build looks for the player beside the `redgem`
program that builds: `redgem-player` in the same folder for the system you are on, and
`runtimes/<system>/` in that folder for the others (`runtimes/windows/redgem-player.exe`,
`runtimes/android/<abi>/libredgem.so`). When one is missing the build says which file it looked
for.

## The game's files

A build packs every file of the game that ships (its scenes, scripts, pictures, sounds, music,
fonts, `game.json` and `locales/`) into one archive, `game.rgpak`, beside the executable. A file
that compresses is compressed (the scripts, the JSON, most models); one that does not (a PNG, an
OGG) is stored as it is. The player reads everything out of it: loose files are easy to change by
mistake or on purpose, look unfinished, and stores prefer few files. What ships is the same as
before: no hidden files and folders (`.git`, `.env`), no editors' backups, no files a link points
at, no signing keys, no `AGENTS.md` / `CLAUDE.md` at the top of the game.

`--loose` keeps the files as they are instead, in a `game/` folder beside the executable, to look
into a build while you debug it:

```bash
./build/src/redgem build my_game dist/my_game --loose
```

The player plays whichever of the two it finds (a packed build takes away the `game/` folder an
earlier loose build left in the same place, and a loose build the archive). A build that fails
leaves the earlier one as it was: the archive is written as `game.rgpak.partial` and renamed
over the old one, and a loose build copies into `game.staged` before swapping it in for `game/`
(the old folder set aside as `game.replaced` meanwhile). The editor's Play,
`redgem <game> --play`, `redgem run` and the other tools always read the project folder itself:
the archive is only made by a build.

The archive is read-only. Saves, settings, bindings and crash reports are kept in the players'
own folder (see [Notes](#notes)), never in it. A player refuses an archive whose header or table
is damaged or was tampered with (a wrong header, a table cut short, sizes out of bounds, a name
that reaches outside the game, a name given twice) and says why. Each file's checksum is checked
when the file is read: a file whose bytes do not match is never handed to the game, and the error
(a script's error, or a warning in the log for a picture or a sound) says the archive is damaged.
Names in the archive are matched exactly, case included, on every system: a script that asks
for `Player.rb` when the file is `player.rb` works loose on Windows and macOS, but not packed.
It is not encryption: anyone with the file and the format below can list and extract it.

The format (`src/core/game_archive.hpp` has it in full; every number is little-endian):

```
header  magic "RGPAK" 0x1A 0x0A 0x00 (8 bytes), version (u32, 1), file count (u32),
        table offset (u64, 32: right after the header), table size (u64)
table   one record per file: name length (u16), method (u8: 0 stored, 1 raw deflate),
        reserved (u8, 0), CRC32 of the file (u32), data offset (u64), size (u64),
        stored size (u64), then the name (UTF-8, '/' between folders: "scripts/player.rb")
data    every file's data, one after another
```

## Icon, version and splash picture

A game says in its `game.json` what it is shown by; all of it is optional:

```json
{
  "title": "My Game",
  "version": "1.2.0",
  "icon": "images/icon.png",
  "splash": "images/splash.png",
  "network": true
}
```

- `version` is a [semantic version](https://semver.org): three numbers, then optionally a
  pre-release (`2.0.0-beta.1`) or build part (`1.0.0+build.5`). Scripts read it with
  `Game.version` (`nil` when there is none). Android shows it as the app's version, and a Mac app
  carries its three numbers. A game without one is version 1.0 there.
- `icon` is a square PNG in the game's folder, 1024×1024 at best (it is scaled down to every size
  a system wants; a smaller one is scaled up, and looks it). Every build turns it into its
  system's own icon (see each system below). A game without one keeps the engine's icon on
  Windows and has the system's default icon elsewhere.
- `splash` is a PNG in the game's folder shown over the whole window, fitted with black bars
  around it, from when the window opens until the first frame of the game is drawn: while the
  world is built, the scripts start and the scene goes to the graphics card. (Reading the scene's
  file happens before the window opens: a big level still takes that long with no window.) It is
  shown by built games and `--play`, not by the editor.
- `network` says whether the game plays online (`Network`) or makes web requests (`HTTP`): its
  Android app asks for the network only when it does, since an app that asks for what it does not
  use worries players and stores. When `game.json` does not say, a build looks at the scripts:
  one that calls `Network.` or `HTTP.` gets the network, and the build warns to say
  `"network": true`. `"network": false` keeps the app offline even then (with a warning). On
  computers it changes nothing.

A build refuses a `game.json` that says one of these and cannot be used (a version that is not
one, a picture that is not a PNG in the game's folder, an icon that is not square), saying which.
A game played that way logs it as a warning and plays as if the line were not there.

## For Windows

On Linux, RedGem builds games for Linux, Windows and Android (the experimental Windows download builds Windows games, the macOS one Mac games: `--target macos`, see [For macOS](#for-macos)). In the editor, **Build**
asks which one; from a terminal:

```bash
tools/windows/build_runtime.sh                                   # once: builds the player for Windows
./build/src/redgem build my_game dist/my_game_win --target windows
```

The first command downloads a Windows compiler into the project (no administrator rights
needed) and builds `redgem-player.exe` into `build/src/runtimes/windows/`; it takes a few minutes
and is only needed once, or again after updating RedGem. The build then makes `dist/my_game_win/my_game.exe` next to its `game/` folder.
The game's icon goes inside the `.exe` (at 16 to 256 pixels): the file manager, the taskbar and
shortcuts show it. Nothing has to run on Windows for that: the build puts the pictures into its
copy of the player, in place of the engine's. The player RedGem makes is not signed; a player
that was would lose its signature there (it would no longer match), so a game you release is
signed afterwards, by you. A player that cannot take the icon (one made without an icon of its
own, or with no room left in its headers) is left as it is, and the icon goes beside it as
`my_game.ico`, with a warning.
It needs 64-bit Windows 10 or later and a graphics card with OpenGL 3.3; nothing has to be
installed with it.

## For Linux

A build for Linux also writes `my_game.desktop`, a menu entry, and the icon as `my_game.png`
(256 pixels) beside the executable. The entry names the program and its icon by name
(`Exec=my_game`, `Icon=my_game`), the way a package installs them: an AppImage, a Flatpak or a
distribution's package takes both files as they are. On its own, the folder runs from a file
manager or a terminal; the entry needs installing to show up in a menu.

The executable is a copy of the `redgem-player` that came with the editor, so it runs where the
editor's download runs: on x86_64 with AVX2 and glibc 2.35 or newer (Ubuntu 22.04, Debian 12,
Fedora 36 and later), whatever system it was built on. It needs no libstdc++ of the player's
system (that is inside it): to start, it needs only glibc (`libc`, `libm`) and `libgcc_s`, which
every Linux system has; OpenGL and X11 are opened when its window opens. The release workflow
checks this on every release: it builds a demo with each Linux download on a bare Ubuntu 22.04
(glibc 2.35), and measures what the built game takes from glibc (`tools/release/glibc_floor.sh`). A player you build from source yourself needs the
glibc of the system it was built on.

## For macOS

On a Mac, `--target macos` (the default there) makes an app: `dist/my_game/My Game.app`, named
after the game's title. It holds everything, so it can be moved and opened like any Mac app:

```
My Game.app/Contents/Info.plist              its name, id, version and icon
My Game.app/Contents/MacOS/my_game           the player
My Game.app/Contents/Resources/game.rgpak    the game's files (game/ with --loose)
My Game.app/Contents/Resources/my_game.icns  the icon (16 to 1024 pixels)
My Game.app/Contents/Resources/licenses/
```

The app's id is the game's `id` (with `_` written as `-`), or `com.redgem.` and its name. The
build signs the app ad hoc (with no identity), which an Apple Silicon Mac requires to open it.
That is not a developer's signature: a Mac that downloads the app warns that its developer cannot
be verified until it is signed with a paid Apple Developer ID and notarized, which the build does
not do.

## For Android

```bash
tools/android/build_runtime.sh                                   # once: builds the player for Android
./build/src/redgem build my_game dist/my_game_android --target android
```

The result is `dist/my_game_android/my_game.apk`, with the game's files inside it: its
`game.rgpak` is one asset of the app, unpacked into the app's own storage on the first run (and
after each update) and read from there. With `--loose` the app carries the files one by one
instead, as before. Install it on
a phone with `adb install`, or copy the file to the phone and open it.
`tools/android/run_on_device.sh my_game` builds, installs, starts the game on the phone or
emulator that `adb` sees, and shows its log.

What it needs on your machine:

- The **Android SDK** with its build-tools and one platform (`sdkmanager "build-tools;35.0.0"
  "platforms;android-35"`), and `ANDROID_HOME` pointing at it.
- **Java** (the SDK's signing tool runs on it).
- The first command downloads the Android compiler (NDK) into the project and builds the engine
  for phones (arm64) and for emulators (x86_64); it takes several minutes, once.

What changes on a phone:

- The game fills the screen: it keeps the height from `game.json` and gets the width that fits.
- **Touch controls** appear by themselves and act as a gamepad: a stick under the left thumb,
  A and B under the right, Start in the corner. A game that plays with a gamepad plays on a phone
  as it is; taps also work as mouse clicks, so `UI.button` menus work. A game with a touch
  interface of its own hides them with `Input.touch_controls = false`.
- Characters may have as many bones on a phone as on a computer (up to 1024 per skeleton).
- The system's **on-screen keyboard** comes up while a `UI.text_field` is drawn, and the back key
  puts it away. It covers the bottom of the screen: keep fields above it (`UI.keyboard_visible?`
  says when it is up).
- Phones are slower than computers: keep levels light (few lights, modest textures) and test on
  a real device. It needs Android 8 or later with OpenGL ES 3.
- The APK is signed with a key the engine makes on your machine the first time
  (`~/.config/redgem/android-debug.keystore`). It never leaves your machine and is different on
  everyone's; it is good for your own devices and for sharing the file.

- The app's launcher icon is made from the game's `icon`, at every screen density (48 to 192
  pixels). Its version is the game's `version` unless the `android` section gives another.
- The app asks for the network (the `INTERNET` permission) when `game.json` says
  `"network": true`, or says nothing and the scripts call `Network.` or `HTTP.` (see above).
  Without it, `Network` and `HTTP` cannot reach anything on a phone.

### For Google Play: an App Bundle

Google Play takes App Bundles (`.aab`), not APKs. `--bundle` makes one:

```bash
./build/src/redgem build my_game dist/my_game_play --target android --bundle
```

It makes `dist/my_game_play/my_game.aab`, signed with the same key an APK would be (for Google
Play, your own: see below). An App Bundle is made with Google's
[bundletool](https://github.com/google/bundletool/releases), which is not part of the SDK: put a
`bundletool` program on the `PATH`, or download `bundletool-all-<version>.jar` and point
`REDGEM_BUNDLETOOL` at it (it runs with Java). `zip` and `unzip` must be installed too.
A bundle is not installed on a phone by itself: `bundletool build-apks` makes the APKs Play would.

### Your own app id, version and key

A game says these in the `android` section of its `game.json`; all are optional:

```json
{
  "title": "My Game",
  "android": {
    "package": "com.mystudio.mygame",
    "version_code": 3,
    "version_name": "1.2",
    "keystore": "../keys/release.keystore",
    "key_alias": "mygame"
  }
}
```

- `package` is the app's id on the phone. Without it the app goes by the game's `id` (see below),
  and a game with no `id` by its folder name.
- `version_code` goes up with every release: a phone only updates to a higher one.
- `version_name` is the version players see; without it, the game's own `version` (1.0 when it
  has none).
- `keystore` and `key_alias` sign the app with a key of your own, which is what a release needs:
  players can only update a game signed with the same key as the one they have. Make it once with
  Java's `keytool` and keep it safe (and backed up): there is no getting it back.
- The key's password is never written in a file. Give it when building:

```bash
REDGEM_KEYSTORE_PASSWORD='...' ./build/src/redgem build my_game dist/android --target android
```

  (`REDGEM_KEY_PASSWORD` too, when the key has a password of its own.)
- Keep the keystore outside the game's folder. If one is left there anyway, builds leave it out:
  a signing key never travels with a game.

## Recording a video

`redgem <game_dir> --record out.mp4` plays the game into a video. You need
[ffmpeg](https://ffmpeg.org) on the `PATH`; without it RedGem says so and does not start.

```
redgem games/arena --record trailer.mp4 --record-fps 30 --record-size 3840x2160 --record-scale 1920x1080 --record-seconds 20
```

- Each frame moves the game on by exactly 1/`--record-fps` seconds (30 unless given), however
  long it takes to draw. A scene too heavy to play smoothly still comes out smooth in the video,
  and so do the wind, film grain and everything else that moves with time.
- `--record-size` is the size the game is drawn at, off screen (the window's own size unless
  given). It is not limited by the window or the desktop, and the window shows a fitted copy.
  `--record-scale` sets the video's size; frames are scaled down to it with lanczos, so drawing
  at 4K and scaling to 1080p gives very clean edges.
- Recording ends after `--record-seconds`, when a script calls `Game.stop_recording` (the game
  keeps running in the window), or when the game quits.
- Frames are read back from the graphics card a few frames late, so drawing never waits for it.
  They are encoded as H.264 (`-crf 16`, preset `medium`, `yuv420p`), which plays everywhere.

## Notes

- The executable needs `game.rgpak` (or, built with `--loose`, the `game/` folder) next to it.
- `game.json` says what the game is and how its window opens:

```json
{ "id": "com.mystudio.mygame", "studio": "My Studio", "title": "My Game", "window_width": 1280, "window_height": 720 }
```

- `studio` and `title` name the folder the game's players keep their saves, settings, bindings
  and crash reports in, where their system keeps what programs remember:

  ```
  Linux     $XDG_DATA_HOME/<studio>/<title>, else ~/.local/share/<studio>/<title>
  Windows   %APPDATA%\<studio>\<title>
  macOS     ~/Library/Application Support/<studio>/<title>
  Android   the app's own files folder
  ```

  `redgem new` writes `"studio": "My Studio"`: put your own in before the first release, and
  leave both alone after. A game whose studio or title changes looks for its saves in a new
  folder, and finds none: nothing is moved. A game that names no studio is kept under
  `RedGem Games`; one with no title goes by its folder's name, which for a built game is the
  folder the player put it in, so moving it loses the saves. A build warns of a game with no
  studio, no title, or the studio `redgem new` wrote. Letters a folder's name cannot hold on some system
  (`/ \ : * ? " < > |`, control characters) become `_`, a name Windows keeps for a device
  (`CON`, `NUL`, `COM1`...) is given a `_` after it, and a long name is cut at 64 bytes. Two games
  of the same studio and title share a folder: a game started as a copy of another needs a title
  of its own.
- The editor's Play and `redgem play` never use that folder: they keep the game's save and
  bindings in `game-data/<studio>/<title>/` of your RedGem folder, and `redgem-test` in
  `test-saves/`, so playing your game while you make it leaves your players' folder alone.
- `id` is what the game is known by: its Android app is named after it. `redgem new` writes one
  (`my-game-3fa2c1b0`); change it to your own before the first release and leave it alone after.
  It is small letters, digits, `.`, `-` and `_`, starting and ending with a letter or a digit.
- `game.json` may also say how the game is shown until its player chooses otherwise:
  `"fullscreen": true`, `"vsync": false`, `"fps_cap": 144` (`0` for no cap), `"render_scale": 0.75`
  (the world drawn at 75% of the window's size) and `"quality": "medium"` (`low`, `medium`, `high`,
  `ultra`). What the player chooses (see `Settings` in the [API reference](scripting/api.md)) is
  kept in `settings.json` in the game's folder (see `studio` above). A built game also reads the
  graphics card (`"gpu"`) and the language (`"language"`) from that file, and never RedGem's own
  settings.

- Everything in the game folder that ships is packed, including the scripts, as their `.rb`
  source (compressed, not compiled nor encrypted).
- Check the licenses of the models and sounds you include. A license that allows using an asset
  in a game does not always allow shipping its original file.

## Crash reports and symbols

A built game that crashes writes a report in `crashes/` of its players' folder (see `studio`
above; on Android, the app's own files folder, and also in `adb logcat`). The next run of the
game says in its log that the run before crashed, naming the report. See
[Troubleshooting](troubleshooting.md) for what a report says.

The player inside every game is stripped of its symbols, so a report's backtrace is offsets into
the program (`Arena.exe+0x1a2b`). Its `Build:` line names the build of the player the game was
built with. Each release keeps the players as they were linked, before they were stripped, beside
its downloads: `redgem-symbols-linux.zip`, `redgem-symbols-windows.zip` and
`redgem-symbols-android.zip` (one per phone processor). To read a report a player sent you:

```bash
unzip redgem-symbols-windows.zip            # from the release your game was built with
redgem symbolize crash-20261024-130509.log --symbols redgem-symbols
```

It prints the report again with the function, and the file and line where the symbols have them,
after each frame of the game's program:

```
  Arena.exe+0x1a2b  in redgem::Engine::step() at src/app/engine.cpp:120
```

`--symbols` takes a symbols file or a folder, searched with the folders in it (the current folder
unless given); the file whose build id is the report's is used. A report of another build says so
and lists the builds it found: get the symbols of the release the game was built with. A player
you built yourself has its symbols in your build folder (`build/src/redgem-player`,
`build-windows/src/redgem-player.exe`, `build-android/<processor>/src/libredgem.so`).
Addresses are looked up with `llvm-addr2line` (LLVM) or `addr2line` (binutils), the first on the
`PATH`, or the program `--tool` names. Reports of a Mac build name no build yet.

## Installing RedGem somewhere else

`cmake --install build --component redgem --prefix <folder>` copies the programs out of the
build folder, laid out the way builds expect:

```
<folder>/bin/redgem            the editor, the project hub and the command line
<folder>/bin/redgem-player     what a game built for this system is a copy of
<folder>/bin/redgem-relay      the meeting point for games over the internet
<folder>/bin/redgem-server     runs a game on a server, for players to join (see Dedicated server)
<folder>/bin/redgem-test       the tool that runs a game's tests
<folder>/bin/runtimes/         the players for other systems that were built before installing
```
