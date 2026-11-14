# The interface keeps its share of the screen when the window changes size: the platformer lays it
# out for its own window (1280x720), so there a unit is a pixel, and in a window half as big each
# unit takes half a pixel, the pause menu's buttons stay where they were in units, and a click on
# one, landing on it where it is drawn, works. A screenshot at each size shows the same menu.
SETTLE_FRAMES = 30
RESIZE_FRAMES = 60          # the most the window may take to come to its new size
REFERENCE = [1280, 720].freeze
SMALL = [640, 360].freeze
HALF = 0.5
SCALE_TOLERANCE = 0.001
RUNNING = 1.0               # Time.scale while the game goes on
RESUME_BUTTON = 0

SystemTest.define do |t|
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("the interface is laid out for the game's own window") { UI.reference == REFERENCE }
  t.assert("at that size a unit is a pixel") { (UI.scale - 1.0).abs < SCALE_TOLERANCE }
  t.press(:p)
  t.wait
  box = menu.button_box(RESUME_BUTTON)
  t.screenshot("pause_menu_at_720p")

  Settings.resolution = SMALL
  t.wait_until(frames: RESIZE_FRAMES, message: "the window never took the size asked for") do
    [UI.pixel_width, UI.pixel_height] == SMALL
  end
  t.wait
  t.assert("half the window, half a pixel a unit") { (UI.scale - HALF).abs < SCALE_TOLERANCE }
  t.assert("the screen is as many units as before") { [UI.width, UI.height] == REFERENCE }
  t.assert("the menu's buttons are where they were, in units") { menu.button_box(RESUME_BUTTON) == box }
  t.screenshot("pause_menu_at_360p")

  x, y, width, height = box
  t.click(x + width / 2, y + height / 2)
  t.assert("a click lands on Resume where it is drawn") { !menu.open? && (Time.scale - RUNNING).abs < SCALE_TOLERANCE }
end
