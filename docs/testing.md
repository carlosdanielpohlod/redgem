---
description: System tests that play the game, Ruby unit tests, and redgem-test.
---

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

Run it with `redgem-test`, the program that runs tests (it is built next to `redgem`):

```bash
./build/src/redgem-test my_game tests/collect_coin_test.rb --screenshots shots
```

The command exits with an error when an assertion fails or a script raises, so it fits in any
automated check. Add `--headless` to run without opening a window (no screenshots then);
`redgem-test --help` lists every option.

To run every test of a game at once, each system test with no window, and get a line per file
(`PASS` or `FAIL` with the first error it said):

```bash
./build/src/redgem-test --all my_game
```

`redgem check my_game --run-tests` does the same after looking the game over (see
[The command line](command-line.md)).

`redgem-test` is a development tool. It is not part of a built game: the program your players
receive (what `redgem build` and the editor's Build make) contains no test code at all, and
`SystemTest` and the rest of what tests are written with do not exist in the Ruby your game
runs in. They are only there while `redgem-test` plays it.

## What a test can do

| Call | Effect |
|------|--------|
| `t.wait(frames)` | Lets the game run |
| `t.hold(:w, :a, frames: 60)` | Holds keys for that long |
| `t.press(:space)` | Taps a key |
| `t.move_mouse(x, y)`, `t.click(x, y)` | Moves the mouse; clicks there with the left button |
| `t.type("Ana")` | Types a text (what `Input.text` and `UI.text_field` read) |
| `t.tilt(:left_y, -1.0, frames: 60)` | Pushes a gamepad stick (or pulls a trigger) for that long |
| `t.press_button(:a)`, `t.hold_button(:a, frames: 30)` | Taps or holds gamepad buttons |
| `t.hold_until(:w, frames: 300) { ... }` | Holds keys until the block is true |
| `t.wait_until(frames: 120) { ... }` | Waits until the block is true, or fails |
| `t.assert("message") { ... }` | Fails the test when the block is false |
| `t.screenshot("name")` | Saves a picture of the next frame as `name.png` in the screenshots folder (`--screenshots <dir>`, `screenshots` unless said) |

Inside the blocks you have the whole scripting API, so a test can also move objects or read any
behaviour's state. Taking pictures is something only tests do: a game's own scripts have no call
for it. What the game saves with `Save` goes to a file of the test's own, which starts
empty: a test never sees or changes your real progress.

## Testing Ruby code by itself

Code that needs no running game (a formula, a helper class) can be tested without one. A file of
unit tests defines classes that inherit from `RedGemTest::Case`; every method whose name starts
with `test_` is a test:

```ruby
# tests/score_test.rb
class ScoreTest < RedGemTest::Case
  def test_a_coin_is_worth_ten_points
    assert_equal 10, 1 * 10
    assert_in_delta 0.5, Vector3.new(0.5, 0, 0).length
  end
end
```

```bash
./build/src/redgem-test --ruby tests/score_test.rb
```

It runs in the same Ruby a game runs in, with the scripting API and an empty scene. The
assertions are `assert`, `refute`, `assert_equal`, `assert_nil`, `assert_in_delta`,
`assert_vector` and `assert_raises`; a system test has them too.

To test the game's own classes, say which game they are from: they then load by name from its
`scripts` folder, as they do when the game runs (without `--game`, from the test file's folder).

```bash
./build/src/redgem-test --ruby tests/score_test.rb --game .
```

A test may reopen one of the game's classes to add to it, a trace or a stand-in for a method:

```ruby
# tests/fighter_trace_test.rb
class Fighter
  alias untraced_strike strike
  def strike(*args) = (Debug.log("strike #{args.inspect}"); untraced_strike(*args))
end
```

A `class` or `module` line at the top of the file (not indented) loads that class from the game's
scripts first, so the line reopens the game's `Fighter` instead of making a new, empty one.
