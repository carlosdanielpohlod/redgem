# Known limitations

RedGem is experimental. This is what is missing or known to be wrong today: good places to
start if you want to [contribute](CONTRIBUTING.md). If you fix one, remove it from this list in
the same pull request.

## Platforms

- The editor is developed and tested on Linux. The Windows download is compiled from Linux and
  has only been tried under Wine, never on a real Windows machine.
- The macOS downloads have only run on GitHub's virtual Macs, which have no graphics card OpenGL
  can use: there the editor is drawn by Apple's software renderer (its system test takes about
  half an hour instead of a minute), and the games' system tests are not run on the Mac. Never
  tried on a real Mac. The app is not signed by a registered Apple developer nor notarized.
- The editor on a Mac builds games for the Mac only: it carries no players for other systems.
  A built Mac game is a plain program beside its files, not an .app.
- The Linux programs need a processor with AVX2 and glibc 2.39 or newer.
- Android games have been run on an emulator only, not on phones. Characters are limited to 48
  bones there. The touch controls have one fixed layout (a stick and three buttons).
- Once, right after an emulator cold boot, an Android game stopped in its audio thread. It did
  not happen again in the runs that followed and has no fix.

## Rendering and animation

- No blend trees or animation state machines: scripts choose and mix clips themselves.
- Particles are not lit and cast no shadows.
- Images must be PNG or JPEG, at most 8192 pixels a side.

## Multiplayer

- Nobody runs a public relay or server: to play over the internet someone has to run
  `redgem-relay` or `redgem-server` on a machine the players can reach.
- A dedicated server opens a room's game when its first player arrives, which holds up the
  server's other rooms for a moment (the time a level takes to load). Its rooms share one
  processor core.
- Messages are JSON over TCP. There is no prediction or lag compensation: fast action games
  will feel the delay.
- A relay accepts 24 connections and 8 rooms per address, and a game hosted directly lets 4
  machines per address wait to join at the same time. Players behind one shared address (a
  campus, some mobile networks) can run into both.
- A room on a relay lets everyone in: connection approval (`Network.authenticate`) works with a
  player hosting directly and on a dedicated server only.
- A client of version 0.2.0 joining a game that decides who comes in gives up after 5 seconds
  if the game takes longer to decide, and is told no reason when refused.
- A dedicated server's `Save` is one file rewritten whole on every change, and its arrays and
  hashes hold at most 4096 entries each: fine for a few thousand players, not for more.

## Web requests (HTTP)

- Redirects are not followed; there are no cookies, no compression, no streaming, no uploads of
  files: a request and its answer are each held whole in memory (at most 8 MB).
- Looking a name up cannot be cut short: a request to a name the network takes long to answer
  for can outlast its timeout by that long (and a game that ends waits for nobody: the request's
  thread ends on its own).
- On Windows the certificates come from the system's ROOT store, which Windows fills on demand:
  an authority no program on the machine has used yet may be missing. Not tried on a real Windows
  machine, nor on a real Android device (whose system certificates the engine reads).
- `http://` names are checked once connected: a public address is never sent anything, but the
  connection to it is made.

## Scripting

- Scripts run on mruby, not on the Ruby you install on your machine: no gems, no `require`, no
  files, sockets or `eval` (on purpose: a game cannot touch the player's machine).
- `a&.b.c` does not stop at `a` being nil as it does in other Rubies: write `a&.b&.c`.
- All text the engine and editor show is English. Games translate their own texts.

## Editor

- A long session with many model imports uses more and more memory until the editor is closed:
  what was deleted is kept for undo.
- Lights and cameras of deleted objects keep their entries in the saved scene file.
- Typing a value into an Inspector field (Ctrl+click) and then clicking another object may give
  the value to the newly selected object. Seen in the code, not confirmed by a test.
- In Docker the editor has no sound.

## Loading and files

- A scene has limits per part (elements, nodes, depth, joints) but no limit on the memory it
  needs as a whole: a huge scene is not refused, it runs out of memory.
- While a game is being developed, symbolic links inside its asset folders are followed. Built
  games do not carry them.

## Tools

- `redgem-test --ruby <file>` cannot load a game's own classes, only the engine's.
- The Blender scene builders do not write the same bytes twice (vertex and UV order change
  between runs), so regenerating a demo scene shows up as a change.
- The showcase demo is built from downloaded assets that may not be passed on, so it is not in
  the downloads and its tests are skipped without it.
- The scripts that record the site's video are not part of the repository.
