# What this game adds to the engine's menus (GameMenu): a Credits screen, reached from the title
# screen, and what shows once Play closes the menu. That is where your game starts: draw it in
# `update`, or load a scene of its own with Game.load_scene. Esc, P or a gamepad's Start opens the
# pause menu again.
class MenuFlow < Behaviour
  CREDITS_INDEX = 2   # on the title screen: after Play and Settings
  TEXT_SIZE = 28

  def start
    @menu = GameMenu.instance
    @menu.screens[:credits] = CreditsScreen.new(@menu)
    label = -> { I18n.t("credits.option") }   # (looked up as it is shown, in the language in use then)
    @menu.screens[:title].add_option(label, index: CREDITS_INDEX) { @menu.show(:credits) }
  end

  def update(_dt)
    return if @menu.open?

    UI.text(I18n.t("play.starts_here"), x: UI.width / 2, y: UI.height / 2, size: TEXT_SIZE, align: :center)
  end
end
