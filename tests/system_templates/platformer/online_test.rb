# A game just made from the platformer template offers to play online from its title screen, and
# says where the server is to be set while there is none.
SETTLE_FRAMES = 30
PLAY_ONLINE_BUTTON = 1

SystemTest.define do |t|
  menu = GameMenu.instance
  online = Camera.main.get_component(Online)
  t.wait(SETTLE_FRAMES)
  t.assert("the title screen offers Play online") do
    menu.screen == :title && menu.screens[:title].options[PLAY_ONLINE_BUTTON].first == "Play online"
  end

  x, y, width, height = menu.button_box(PLAY_ONLINE_BUTTON)
  t.click(x + width / 2, y + height / 2)
  t.wait
  t.assert("with no server set, it starts the game and says where to set one") do
    !menu.open? && online.notice == I18n.t("online.no_server") && !Network.connecting? && !Network.online?
  end
  t.screenshot("no_server_yet")
end
