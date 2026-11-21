# A theme game.json gives (its "menu" section) changes the menus with no script: a heading font of
# fonts/, buttons on a picture cut in nine (another for the focused one), an icon on Play, the
# column on the right, the screens fading in; the title screen with a logo, the version in a
# footer and the scene blurred behind it. Each screen against its picture (baselines/json_*.png).
SETTLE_FRAMES = 30    # (past the title's fade)
SHOWN_FRAMES = 30

SystemTest.define do |t|
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("game.json's theme is the menu's") { menu.theme_of(:title)[:button, :skin] == "panel" }
  t.assert("the title's own is over it") { menu.theme_of(:title)[:logo, :image] == "logo" }
  t.assert("the scene behind the title is blurred") { (Camera.backdrop_blur - 0.6).abs < 0.001 }
  t.match_baseline("json_title")
  menu.show(:pause)
  t.assert("another screen keeps the scene sharp") { Camera.backdrop_blur.zero? }
  t.wait(SHOWN_FRAMES)
  t.match_baseline("json_pause")
  menu.show_settings
  t.wait(SHOWN_FRAMES)
  t.match_baseline("json_settings")
  menu.resume
  t.wait
  t.assert("the game goes on as it was") { Camera.backdrop_blur.zero? && Time.scale == GameMenu::RUNNING }
end
