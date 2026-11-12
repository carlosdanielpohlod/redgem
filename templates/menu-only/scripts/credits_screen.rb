# The credits: who made the game, in a list that rolls up by itself (UI::ScrollList). The mouse
# wheel, Page Up and Page Down or a gamepad's right stick scroll it too; Back or Esc goes back to
# the title screen. Put your own names in CREDITS; what each role is called is in locales/en.json
# (each a lookup, made as it is shown: in the language in use then).
class CreditsScreen < GameMenu::Screen
  CREDITS = [
    [-> { I18n.t("credits.design") }, "Your name"],
    [-> { I18n.t("credits.programming") }, "Your name"],
    [-> { I18n.t("credits.art") }, "Your name"],
    [-> { I18n.t("credits.music") }, "Your name"],
    [-> { I18n.t("credits.sound") }, "Your name"],
    [-> { I18n.t("credits.writing") }, "Your name"],
    [-> { I18n.t("credits.testing") }, "Your friends"],
    [-> { I18n.t("credits.engine") }, "RedGem"],
    [-> { I18n.t("credits.thanks") }, "You, for playing"]
  ].freeze
  LIST_WIDTH = 520
  LIST_HEIGHT = 300          # (pixels on a 720 pixel high screen: the menu scales it to others)
  ROW_HEIGHT = 64
  ROW_GAP = 8
  LIST_ABOVE_BUTTON = 24     # pixels between the list's bottom and the Back button
  ROLE_SIZE = 18
  NAME_SIZE = 26
  ROLE_COLOR = :gold
  ROLL_SPEED = 25.0          # pixels a second the list rolls up by itself
  BUTTON_SHARE = 0.85        # where the Back button sits, as a share of the screen's height

  # The list, which keeps how far it is scrolled.
  attr_reader :list

  # How far a list scrolled `offset` pixels is once it rolled for `seconds` more.
  def self.rolled(offset, seconds) = offset + ROLL_SPEED * seconds

  def initialize(menu)
    back = -> { menu.show(:title) }
    super(heading: -> { I18n.t("credits.heading") }, options: [[-> { I18n.t("game.menu.back") }, back]], back: back)
    @menu = menu
    @list = UI::ScrollList.new
  end

  def opened = @list.reset
  def first_button_share = BUTTON_SHARE
  def space_above = @menu.scaled(LIST_HEIGHT + LIST_ABOVE_BUTTON)

  # The list, above the Back button (`first_button` is where that is: [x, y, width, height]).
  # It rolls by real time: the game is frozen while a menu is open.
  def draw(first_button)
    width = @menu.scaled(LIST_WIDTH)
    @list.offset = CreditsScreen.rolled(@list.offset, Time.unscaled_delta)
    @list.draw(CREDITS.size, x: (UI.width - width) / 2, y: first_button[1] - space_above, width: width,
                             height: @menu.scaled(LIST_HEIGHT), row_height: @menu.scaled(ROW_HEIGHT),
                             gap: @menu.scaled(ROW_GAP)) { |index, box| credit(CREDITS[index], box) }
  end

  private

  # One row: the role, and under it who did it.
  def credit((role, name), box)
    middle = box[:x] + box[:width] / 2
    UI.text(role.call, x: middle, y: box[:y], size: @menu.scaled(ROLE_SIZE), color: ROLE_COLOR, align: :center)
    UI.text(name, x: middle, y: box[:y] + @menu.scaled(ROLE_SIZE), size: @menu.scaled(NAME_SIZE), align: :center)
  end
end
