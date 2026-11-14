# The player's display settings and controls, changed while the game runs: the window takes the
# size asked for (and keeps it with the world drawn at half of it), the settings screen is reached
# and worked with a gamepad alone, the focus moving with the D-pad (the interface made a quarter
# bigger on the way), and its controls page rebinds
# jump to K (a key, after the gamepad's A picked the cell) and to Y (a button, after a click on the
# cell). Starting the game again keeps all of it: tests/system_restarts/platformer/display_settings_restarted_test.rb.
SETTLE_FRAMES = 30
RESIZE_FRAMES = 60          # the most the window may take to come to its new size
APEX_FRAMES = 20
LANDING_FRAMES = 120
MIN_JUMP_HEIGHT = 1.0
SMALL = [640, 360].freeze
HALF = 0.5
SETTINGS_OPTION = 3         # on the pause menu: Resume, Restart, Multiplayer, Settings, Quit
QUALITY_ROW = 5             # on the settings screen: mode, resolution, scale, vsync, cap, quality
UI_SCALE_ROW = 6            # then the interface's size
LARGER_UI = 1.25            # the size after the game's own (1.0), right of it
CONTROLS_OPTION = 11        # after its eleven rows (seven of video, four volumes): the Controls button, then Back
JUMP_ROW = 2                # on the controls page: the player's choice, then fire, jump, run...
JUMP = GameMenu::ControlsPage::Row.new(:jump, nil)

SystemTest.define do |t|
  menu = GameMenu.instance
  player = Scene.find("Player")
  down = lambda do |times|
    times.times do
      t.press_button(:dpad_down)
      t.wait # the button comes up before it goes down again
    end
  end
  t.wait(SETTLE_FRAMES)
  t.assert("the window is the size the game asks for") { Settings.resolution == [UI.pixel_width, UI.pixel_height] }
  t.assert("its monitor's sizes are offered") { Settings.resolutions.any? }

  Settings.resolution = SMALL
  t.wait_until(frames: RESIZE_FRAMES, message: "the window never took the size asked for") do
    [UI.pixel_width, UI.pixel_height] == SMALL
  end
  Settings.render_scale = HALF
  t.wait
  t.screenshot("half_render_scale")
  t.assert("the world at half the size leaves the interface at the window's") do
    [UI.pixel_width, UI.pixel_height] == SMALL
  end

  t.press_button(:start)
  t.wait
  down.call(SETTINGS_OPTION)
  t.press_button(:a)
  t.wait
  t.assert("A picks the focused option: the settings") { menu.screen == :settings }
  t.screenshot("settings_screen")
  down.call(QUALITY_ROW)
  t.press_button(:dpad_left)
  t.wait
  t.assert("left on the quality's row lowers it") { Settings.quality == :high }

  down.call(UI_SCALE_ROW - QUALITY_ROW)
  t.press_button(:dpad_right)
  t.wait
  t.assert("right on the interface's row makes it bigger") { (Settings.ui_scale - LARGER_UI).abs < Float::EPSILON }
  t.screenshot("settings_larger_interface")

  down.call(CONTROLS_OPTION - UI_SCALE_ROW)
  t.press_button(:a)
  t.wait
  t.assert("the Controls button opens the controls page") { menu.screen == :controls }
  controls = menu.screens[:controls]
  down.call(JUMP_ROW)
  t.press_button(:a)
  t.wait
  t.assert("A on jump's keys waits for a key") { controls.page.waiting == [JUMP, GameMenu::ControlsPage::KEYS] }
  t.screenshot("controls_waiting")
  t.press(:k)
  t.wait
  t.assert("K takes the place of Space") { Input.bindings(:jump) == ["key:k", "pad:a"] }

  cell = controls.cells.fetch([:jump, nil, GameMenu::ControlsPage::BUTTONS])
  t.click(cell[:x] + cell[:width] / 2, cell[:y] + cell[:height] / 2)
  t.wait
  t.press_button(:y)
  t.wait
  t.assert("a click on jump's buttons, then Y, takes the place of A") { Input.bindings(:jump) == ["key:k", "pad:y"] }
  t.screenshot("controls_screen")
  t.press(:escape)
  t.wait
  t.assert("Esc goes back to the settings") { menu.screen == :settings }
  t.press(:escape)
  t.wait
  t.assert("Esc keeps the settings and goes back to the pause menu") { menu.screen == :pause }
  t.press(:escape)
  t.wait(SETTLE_FRAMES)

  ground_y = player.transform.position.y
  t.press(:space)
  t.wait(APEX_FRAMES)
  t.assert("Space no longer jumps") { player.character.grounded? }
  t.press(:k)
  t.wait(APEX_FRAMES)
  t.assert("K jumps") { player.transform.position.y > ground_y + MIN_JUMP_HEIGHT }
  t.wait_until(frames: LANDING_FRAMES, message: "never landed") { player.character.grounded? }
end
