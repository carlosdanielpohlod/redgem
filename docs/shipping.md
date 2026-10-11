# Building and sharing

**Build** in the editor's toolbar, or from a terminal:

```bash
./build/src/redgem build my_game dist/my_game
```

This makes a folder with an executable named after your game (`dist/my_game/my_game`) and a
`game/` folder with its files. Whoever receives the folder runs the executable: they do not need
RedGem, Ruby or Blender installed.

## For Windows

RedGem runs on Linux, and builds games for Linux, Windows and Android. In the editor, **Build**
asks which one; from a terminal:

```bash
tools/windows/build_runtime.sh                                   # once: builds the engine for Windows
./build/src/redgem build my_game dist/my_game_win --target windows
```

The first command downloads a Windows compiler into the project (no administrator rights
needed) and builds `redgem.exe`; it takes a few minutes and is only needed once, or again after
updating RedGem. The build then makes `dist/my_game_win/my_game.exe` next to its `game/` folder.
It needs 64-bit Windows 10 or later and a graphics card with OpenGL 3.3; nothing has to be
installed with it.

## For Android

```bash
tools/android/build_runtime.sh                                   # once: builds the engine for Android
./build/src/redgem build my_game dist/my_game_android --target android
```

The result is `dist/my_game_android/my_game.apk`, with the game's files inside it. Install it on
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
  as it is; taps also work as mouse clicks, so `UI.button` menus work.
- There is no on-screen keyboard yet, so text fields cannot be typed in.
- Phones are slower than computers: keep levels light (few lights, modest textures) and test on
  a real device. It needs Android 8 or later with OpenGL ES 3.
- The APK is signed with a key made on your machine, good for your own devices and for sharing the
  file. Publishing on a store needs your own key and its packaging rules.

## Notes

- The executable needs the `game/` folder next to it.
- `game.json` sets the window title and size:

```json
{ "title": "My Game", "window_width": 1280, "window_height": 720 }
```

- Everything in the game folder is copied, including the scripts as plain `.rb` files.
- Check the licenses of the models and sounds you include. A license that allows using an asset
  in a game does not always allow shipping its original file.
