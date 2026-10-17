# The pause menu's login screen, when the game has a backend (see Accounts): a name, then a
# password, then the backend's answer. (One field at a time: whatever is typed goes to every
# field drawn.)
class LoginScreen < GameMenu::Screen
  STATUS_SIZE = 22
  STATUS_ABOVE_FIELD = 34           # pixels from the status line's top to the field
  FIELD_ABOVE_BUTTONS = 74          # pixels from the field's top to the first button
  HEIGHT_ABOVE_BUTTONS = FIELD_ABOVE_BUTTONS + STATUS_ABOVE_FIELD

  attr_accessor :name, :password

  def initialize(menu)
    super(heading: -> { I18n.t("login.heading") }, back: -> { menu.show(:multiplayer) })
    @name = ""
    @password = ""
    @asking_password = false
  end

  def options = [[advance_label, -> { advance }], [I18n.t("menu.back"), back]]
  def space_above = HEIGHT_ABOVE_BUTTONS
  def typing? = true

  def draw(box)
    x, y, width, height = box
    UI.text(@note || prompt, x: UI.width / 2, y: y - HEIGHT_ABOVE_BUTTONS, size: STATUS_SIZE, color: :gold,
                             align: :center)
    field = { x: x, y: y - FIELD_ABOVE_BUTTONS, width: width, height: height }
    return @password = UI.text_field(@password, secret: true, **field) if @asking_password

    @name = UI.text_field(@name, **field)
  end

  # The next step: from the name to the password, and from the password to the backend.
  def advance
    @note = nil
    return @asking_password = true unless @asking_password

    @note = I18n.t("login.logging_in")
    Accounts.log_in(@name, @password) { |failure| answered(failure) }
  end

  private

  def advance_label = @asking_password ? I18n.t("login.log_in") : I18n.t("login.next")
  def prompt = @asking_password ? I18n.t("login.password") : I18n.t("login.name")

  def answered(failure)
    @password = ""
    @asking_password = false
    @note = failure || I18n.t("login.logged_in", name: Accounts.name)
  end
end
