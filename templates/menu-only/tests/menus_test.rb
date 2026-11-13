# A test that plays the game, frame by frame, with keys the test presses. It goes through the
# menus as a player with a keyboard does: up and down move between the buttons, Enter picks one.
# Run every test of the game with `redgem check . --run-tests` (see the manual's Testing your game).
SETTLE_FRAMES = 10
ROLL_FRAMES = 60             # a second of the credits rolling
SETTINGS_BUTTON = 1          # on the title screen: Play, Settings, Credits, Quit
CREDITS_BUTTON = 2

SystemTest.define do |t|
  menu = GameMenu.instance
  # Presses a key, and lets it go for a frame (a key held down is pressed once).
  tap = lambda do |key|
    t.press(key)
    t.wait
  end
  pick = lambda do |button|
    button.times { tap.call(:down) }
    tap.call(:enter)
  end
  t.wait(SETTLE_FRAMES)
  t.assert("the game opens on its title screen") { menu.screen == :title }
  t.assert("the game is frozen behind it") { Time.scale.zero? }
  t.assert("the title is the game's") { menu.screens[:title].heading == Game.title }

  pick.call(CREDITS_BUTTON)
  t.assert("Credits opens the credits") { menu.screen == :credits }
  t.wait(ROLL_FRAMES)
  t.assert("the credits roll") { menu.screens[:credits].list.offset.positive? }
  tap.call(:escape)
  t.assert("Esc goes back to the title screen") { menu.screen == :title }

  pick.call(SETTINGS_BUTTON)
  t.assert("Settings opens the engine's settings screen") { menu.screen == :settings }
  menu.screens[:settings].focus_controls
  tap.call(:enter)
  t.assert("its Controls button opens the controls page") { menu.screen == :controls }
  tap.call(:escape)
  tap.call(:escape)
  t.assert("Esc goes back, to the title screen") { menu.screen == :title }

  tap.call(:enter)
  t.assert("Play closes the menu: the game goes on") { !menu.open? }
end
