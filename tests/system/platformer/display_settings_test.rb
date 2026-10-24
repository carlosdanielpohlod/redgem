# The player's display settings, changed while the game runs: the window takes the size asked for
# (and keeps it with the world drawn at half of it), and the settings screen is reached and worked
# with a gamepad alone, the focus moving with the D-pad.
SETTLE_FRAMES = 30
RESIZE_FRAMES = 60          # the most the window may take to come to its new size
SMALL = [640, 360].freeze
HALF = 0.5
SETTINGS_OPTION = 3         # on the pause menu: Resume, Restart, Multiplayer, Settings, Quit
QUALITY_ROW = 5             # on the settings screen: mode, resolution, scale, vsync, cap, quality

SystemTest.define do |t|
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("the window is the size the game asks for") { Settings.resolution == [UI.width, UI.height] }
  t.assert("its monitor's sizes are offered") { Settings.resolutions.any? }

  Settings.resolution = SMALL
  t.wait_until(frames: RESIZE_FRAMES, message: "the window never took the size asked for") do
    [UI.width, UI.height] == SMALL
  end
  Settings.render_scale = HALF
  t.wait
  t.screenshot("half_render_scale")
  t.assert("the world at half the size leaves the interface at the window's") { [UI.width, UI.height] == SMALL }

  t.press_button(:start)
  t.wait
  SETTINGS_OPTION.times do
    t.press_button(:dpad_down)
    t.wait  # the button comes up before it goes down again
  end
  t.press_button(:a)
  t.wait
  t.assert("A picks the focused option: the settings") { menu.screen == :settings }
  t.screenshot("settings_screen")
  QUALITY_ROW.times do
    t.press_button(:dpad_down)
    t.wait
  end
  t.press_button(:dpad_left)
  t.wait
  t.assert("left on the quality's row lowers it") { Settings.quality == :high }
  t.press(:escape)
  t.wait
  t.assert("Esc keeps the settings and goes back to the pause menu") { menu.screen == :pause }
end
