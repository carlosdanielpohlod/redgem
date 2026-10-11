# The pause menu leads to the multiplayer screen: an address can be typed, a game hosted and left,
# and Esc goes back (P does not: there it is a letter).
SETTLE_FRAMES = 30
MULTIPLAYER_BUTTON = 2    # on the pause menu
HOST_BUTTON = 0           # on the multiplayer screen
LEAVE_BUTTON = 2
PORT = 47_822             # not the usual one: a game may be open on this machine

SystemTest.define do |t|
  menu = GameMenu.instance
  press = lambda do |index|
    x, y, width, height = menu.button_box(index)
    t.click(x + width / 2, y + height / 2)
  end
  t.wait(SETTLE_FRAMES)
  menu.multiplayer.port = PORT
  t.press(:escape)
  t.wait
  press.call(MULTIPLAYER_BUTTON)
  t.assert("the multiplayer screen is open, nobody is online yet, and it looks for games around") do
    menu.screen == :multiplayer && !Network.online? && Network.searching?
  end

  address = menu.multiplayer.address
  t.type("9pç")
  t.wait
  t.assert("typing goes into the address (P included) and the screen stays") do
    menu.multiplayer.address == "#{address}9pç" && menu.screen == :multiplayer
  end
  3.times do
    t.press(:backspace)
    t.wait  # the key comes up before it goes down again
  end
  t.assert("backspace takes it out again, a whole letter at a time") { menu.multiplayer.address == address }

  press.call(HOST_BUTTON)
  t.assert("Host a game starts one, and the looking stops") do
    Network.host? && Network.id == 1 && Network.port == PORT && !Network.searching?
  end
  press.call(HOST_BUTTON)
  t.assert("hosting again starts over cleanly") { Network.host? && Network.peers.empty? }
  t.screenshot("multiplayer_menu")
  press.call(LEAVE_BUTTON)
  t.assert("Leave the game ends it") { !Network.online? }

  t.press(:escape)
  t.wait
  t.assert("Esc goes back to the pause menu, and nothing is looked for any more") do
    menu.screen == :pause && !Network.searching?
  end
end
