# A game just made from the menu-only template, in a window, worked with the mouse: its title
# screen, its credits, and the engine's settings screen. (The keyboard, and what each screen
# does, are the game's own tests: templates/menu-only/tests/.)
SETTLE_FRAMES = 10
ROLL_FRAMES = 60             # the credits rolling, before their picture
SETTINGS_BUTTON = 1          # on the title screen: Play, Settings, Credits, Quit
CREDITS_BUTTON = 2
BACK_BUTTON = 0              # the credits' only button
GAME_TITLE = "New Game".freeze   # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  menu = GameMenu.instance
  click_button = lambda do |index|
    x, y, width, height = menu.button_box(index)
    t.click(x + width / 2, y + height / 2)
  end
  t.wait(SETTLE_FRAMES)
  t.assert("the title screen is the game's") { menu.screens[:title].heading == GAME_TITLE }
  t.screenshot("title")

  click_button.call(CREDITS_BUTTON)
  t.assert("Credits opens the credits") { menu.screen == :credits }
  t.wait(ROLL_FRAMES)
  t.screenshot("credits")
  click_button.call(BACK_BUTTON)
  t.assert("Back goes to the title screen") { menu.screen == :title }

  click_button.call(SETTINGS_BUTTON)
  t.assert("Settings opens the engine's settings screen") { menu.screen == :settings }
  t.screenshot("settings")
end
