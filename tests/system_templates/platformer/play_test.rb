# A game just made from the platformer template, played from its title screen to its end screen.
SETTLE_FRAMES = 30
WALK_FRAMES = 300
PICKUP_FRAMES = 30
PLAY_BUTTON = 0
GAME_TITLE = "New Game".freeze   # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  menu = GameMenu.instance
  player = Scene.find("Player")
  hero = player.get_component(Player)
  t.wait(SETTLE_FRAMES)
  t.assert("the game opens on its title screen, under its own name, frozen") do
    menu.screen == :title && menu.screens[:title].heading == GAME_TITLE && Time.scale.zero?
  end
  t.screenshot("title_screen")

  x, y, width, height = menu.button_box(PLAY_BUTTON)
  t.click(x + width / 2, y + height / 2)
  t.assert("Play starts the game") { !menu.open? }
  t.hold_until(:w, frames: WALK_FRAMES, message: "the first coin was not reached") { hero.coins == 1 }

  t.press(:p)
  t.wait
  t.assert("P pauses") { menu.screen == :pause && Time.scale.zero? }
  t.press(:escape)
  t.wait
  t.assert("Esc goes on") { !menu.open? }

  Scene.find_all(:coin).each do |coin|
    player.transform.position = coin.transform.position
    t.wait_until(frames: PICKUP_FRAMES, message: "#{coin.name} not collected") { !coin.alive? }
  end
  t.wait
  t.assert("the last coin ends the game") do
    menu.screen == :over && menu.screens[:over].heading == "You got every coin!"
  end
  t.screenshot("end_screen")
end
