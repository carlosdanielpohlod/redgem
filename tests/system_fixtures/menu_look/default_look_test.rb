# The engine's menus with no theme look as they always did: each screen against its picture, taken
# before the menus had themes (baselines/default_*.png in the fixture game).
SETTLE_FRAMES = 10
SHOWN_FRAMES = 3

SystemTest.define do |t|
  menu = GameMenu.instance
  look = lambda do |name|
    t.wait(SHOWN_FRAMES)
    t.match_baseline("default_#{name}")
  end
  t.wait(SETTLE_FRAMES)
  t.match_baseline("default_title")
  menu.show(:pause)
  look.call(:pause)
  menu.show_settings
  look.call(:settings)
  menu.show(:controls)
  look.call(:controls)
  Quests.start(:lamp)
  menu.show(:journal)
  look.call(:journal)
  menu.finish("You win!")
  look.call(:over)
end
