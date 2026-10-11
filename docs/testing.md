# Testing your game

The example below tests the game made in [Your first game](first-game.md).

A system test is a Ruby script that plays your game: it holds keys, waits, checks what happened
and can take screenshots. It runs the real game, frame by frame, at the same pace on any machine.

```ruby
# tests/collect_coin_test.rb
SystemTest.define do |t|
  t.wait(30)                                   # frames
  t.hold(:w, frames: 60)                       # walk forward for a second
  t.wait_until(frames: 30) { Scene.find("Coin1").nil? }
  t.assert("the score counts the coin") { Scene.find("Player").get_component(Player).coins == 1 }
  t.screenshot("coin_collected")
end
```

Run it:

```bash
./build/src/redgem my_game --test tests/collect_coin_test.rb --screenshots shots
```

The command exits with an error when an assertion fails or a script raises, so it fits in any
automated check. Add `--headless` to run without opening a window (no screenshots then).

## What a test can do

| Call | Effect |
|------|--------|
| `t.wait(frames)` | Lets the game run |
| `t.hold(:w, :a, frames: 60)` | Holds keys for that long |
| `t.press(:space)` | Taps a key |
| `t.move_mouse(x, y)`, `t.click(x, y)` | Moves the mouse; clicks there with the left button |
| `t.tilt(:left_y, -1.0, frames: 60)` | Pushes a gamepad stick (or pulls a trigger) for that long |
| `t.press_button(:a)`, `t.hold_button(:a, frames: 30)` | Taps or holds gamepad buttons |
| `t.hold_until(:w, frames: 300) { ... }` | Holds keys until the block is true |
| `t.wait_until(frames: 120) { ... }` | Waits until the block is true, or fails |
| `t.assert("message") { ... }` | Fails the test when the block is false |
| `t.screenshot("name")` | Saves `name.png` in the screenshots folder |

Inside the blocks you have the whole scripting API, so a test can also move objects or read any
behaviour's state. What the game saves with `Save` goes to a file of the test's own, which starts
empty: a test never sees or changes your real progress.
