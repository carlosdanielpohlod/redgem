# A theme a game's script gives (GameMenu.theme and a screen's own) changes every screen: buttons
# left in a column with square corners and a border, another shade, a gradient behind the title,
# and the settings rows and the controls cells in its colors. Each screen against its picture
# (baselines/script_*.png), and far from the engine's own look.
SETTLE_FRAMES = 10
SHOWN_FRAMES = 3
LEAST_CHANGE = 0.05   # of a picture's pixels that the theme must change

AMBER = [150, 90, 20].freeze

SystemTest.define do |t|
  GameMenu.theme = {
    colors: { button: [20, 40, 70], hover: AMBER, focused: AMBER, label: [240, 240, 220], caption: :orange,
              focus_ring: :white, accent: :gold, border: :gold, shade: [10, 20, 40], shade_alpha: 200 },
    button: { radius: 0, border: 2, width: 240 },
    layout: { align: :left, margins: [0, 0, 0, 60] },
    sizes: { heading: 56 }
  }
  menu = GameMenu.instance
  menu.screens[:title].theme = { background: { kind: :gradient, colors: [[60, 20, 80], :black] } }
  look = lambda do |name|
    t.wait(SHOWN_FRAMES)
    t.match_baseline("script_#{name}")
    t.assert("#{name} is not the engine's own look") do
      TestScreen.difference("script_#{name}", "baselines/default_#{name}.png") > LEAST_CHANGE
    end
  end
  t.wait(SETTLE_FRAMES)
  menu.show(:title)
  look.call(:title)
  menu.show_settings
  look.call(:settings)
  menu.show(:controls)
  look.call(:controls)
end
