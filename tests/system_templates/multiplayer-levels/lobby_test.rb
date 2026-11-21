# A game just made from the multiplayer-levels template opens on its title screen, which offers to
# play together: the lobby hosts, joins an address, or joins the game's dedicated server, and says
# where that server is to be set while there is none.
SETTLE_FRAMES = 30
PLAY_BUTTON = 0
TOGETHER_BUTTON = LobbyScreen::OPTION_INDEX
SERVER_BUTTON = 2                 # on the lobby: after Host and Join
GAME_TITLE = "New Game".freeze    # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("the game opens on its title screen, under its own name") do
    menu.screen == :title && menu.screens[:title].heading == GAME_TITLE
  end
  t.assert("the title screen offers Play and Play together") do
    labels = menu.screens[:title].options.map(&:first)
    labels[PLAY_BUTTON] == I18n.t("game.menu.play") && labels[TOGETHER_BUTTON] == "Play together"
  end
  t.screenshot("title_screen")

  x, y, width, height = menu.button_box(TOGETHER_BUTTON)
  t.click(x + width / 2, y + height / 2)
  t.wait
  t.assert("Play together opens the lobby") { menu.screen == :together }
  t.screenshot("lobby")
  t.press(:escape)
  t.wait
  t.assert("Esc goes back to the title screen") { menu.screen == :title }

  menu.screens[:title].options[TOGETHER_BUTTON][1].call
  x, y, width, height = menu.button_box(SERVER_BUTTON)
  t.click(x + width / 2, y + height / 2)
  t.wait
  t.assert("with no server set, the lobby stays open and says where to set one") do
    menu.screen == :together && menu.screens[:together].note == I18n.t("lobby.no_server") && !Network.connecting?
  end
end
