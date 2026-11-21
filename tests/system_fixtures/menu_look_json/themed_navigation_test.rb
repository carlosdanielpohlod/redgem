# A themed menu is worked as any other: down and Enter on the keyboard, the D-pad and A on a
# gamepad, Esc and Start, and the mouse on a button where the theme put it (and its sounds play).
SETTLE_FRAMES = 30
SETTINGS_BUTTON = 1   # on the title screen: Play, Settings, Quit
RESUME_BUTTON = 0

SystemTest.define do |t|
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("the game opens on its themed title screen") { menu.screen == :title }

  t.press(:down)
  t.press(:enter)
  t.wait
  t.assert("down and Enter pick Settings") { menu.screen == :settings }
  t.press(:escape)
  t.wait
  t.assert("Esc goes back to the title screen") { menu.screen == :title }

  t.press_button(:dpad_down)
  t.press_button(:dpad_up)
  t.press_button(:a)
  t.wait
  t.assert("the D-pad and A pick Play") { !menu.open? && Time.scale == GameMenu::RUNNING }

  t.press_button(:start)
  t.wait
  t.assert("Start opens the themed pause menu") { menu.screen == :pause }
  x, y, width, height = menu.button_box(RESUME_BUTTON)
  t.assert("its buttons are on the right, inside the margin") { x + width == UI.width - menu.margin(:right) }
  t.click(x + width / 2, y + height / 2)
  t.assert("a click on Resume works where the theme put it") { !menu.open? }
end
