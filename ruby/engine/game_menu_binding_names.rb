# What the menu's controls page (see GameMenu::ControlsScreen) calls things, in the language in
# use: a binding ("key:space" is "Space", "pad:a" "A button"), a stick (:left_x is "Left stick X")
# and a row (the action :jump is "Jump", the negative side of :horizontal "Move left").
class GameMenu < Behaviour
  module BindingNames
    # Keys with a word for a name. Letters show as capitals, digits as they are.
    KEYS = { space: -> { I18n.t("game.controls.key_space") },
             escape: -> { I18n.t("game.controls.key_escape") },
             enter: -> { I18n.t("game.controls.key_enter") },
             tab: -> { I18n.t("game.controls.key_tab") },
             backspace: -> { I18n.t("game.controls.key_backspace") },
             up: -> { I18n.t("game.controls.key_up") },
             down: -> { I18n.t("game.controls.key_down") },
             left: -> { I18n.t("game.controls.key_left") },
             right: -> { I18n.t("game.controls.key_right") },
             left_shift: -> { I18n.t("game.controls.key_left_shift") },
             right_shift: -> { I18n.t("game.controls.key_right_shift") },
             left_control: -> { I18n.t("game.controls.key_left_control") },
             right_control: -> { I18n.t("game.controls.key_right_control") },
             left_alt: -> { I18n.t("game.controls.key_left_alt") } }.freeze
    # Keys shown as the character they type.
    CHARACTERS = { apostrophe: "'", comma: ",", minus: "-", period: ".", slash: "/" }.freeze
    BUTTONS = { a: -> { I18n.t("game.controls.button_a") },
                b: -> { I18n.t("game.controls.button_b") },
                x: -> { I18n.t("game.controls.button_x") },
                y: -> { I18n.t("game.controls.button_y") },
                left_bumper: -> { I18n.t("game.controls.button_left_bumper") },
                right_bumper: -> { I18n.t("game.controls.button_right_bumper") },
                back: -> { I18n.t("game.controls.button_back") },
                start: -> { I18n.t("game.controls.button_start") },
                dpad_up: -> { I18n.t("game.controls.button_dpad_up") },
                dpad_down: -> { I18n.t("game.controls.button_dpad_down") },
                dpad_left: -> { I18n.t("game.controls.button_dpad_left") },
                dpad_right: -> { I18n.t("game.controls.button_dpad_right") },
                left_stick: -> { I18n.t("game.controls.button_left_stick") },
                right_stick: -> { I18n.t("game.controls.button_right_stick") } }.freeze
    STICKS = { left_x: -> { I18n.t("game.controls.stick_left_x") },
               left_y: -> { I18n.t("game.controls.stick_left_y") },
               right_x: -> { I18n.t("game.controls.stick_right_x") },
               right_y: -> { I18n.t("game.controls.stick_right_y") },
               left_trigger: -> { I18n.t("game.controls.stick_left_trigger") },
               right_trigger: -> { I18n.t("game.controls.stick_right_trigger") } }.freeze
    # The rows of the engine's own actions and axes (a game's text of the same key wins).
    ROWS = { jump: -> { I18n.t("game.controls.action_jump") },
             fire: -> { I18n.t("game.controls.action_fire") },
             run: -> { I18n.t("game.controls.action_run") },
             horizontal_negative: -> { I18n.t("game.controls.action_horizontal_negative") },
             horizontal_positive: -> { I18n.t("game.controls.action_horizontal_positive") },
             vertical_negative: -> { I18n.t("game.controls.action_vertical_negative") },
             vertical_positive: -> { I18n.t("game.controls.action_vertical_positive") } }.freeze
    # What a game's text for a row of its own is under (apart from the page's other texts).
    ROW_KEY_PREFIX = "game.controls.action_".freeze
    SIDE_MARKS = { negative: " -", positive: " +" }.freeze  # after a readable axis name
    WORD_JOINER = "_".freeze                   # between the words of a name

    # A binding by name: "key:left_shift" is "Left Shift".
    def self.of(binding)
      kind, name = binding.split(":", 2)
      "#{kind}:" == Input::PAD_PREFIX ? button(name.to_sym) : key(name.to_sym)
    end

    def self.key(name) = KEYS[name]&.call || CHARACTERS[name] || name.to_s.upcase
    def self.button(name) = BUTTONS[name]&.call || readable(name)
    def self.stick(axis) = STICKS[axis]&.call || readable(axis)

    # A row's name: an action's (side nil) or one side of an axis'. A game names its own in its
    # translation files, as game.controls.action_<name> (action_<name>_<side> for a side); without
    # one, the name is made readable (:steer's sides are "Steer -" and "Steer +").
    def self.row(name, side)
      key = side ? :"#{name}#{WORD_JOINER}#{side}" : name.to_sym
      ROWS[key]&.call || games_own(key) || "#{readable(name)}#{SIDE_MARKS[side]}"
    end

    # The game's text for a row, if it has one (a key with no text is shown as itself).
    def self.games_own(key)
      wanted = "#{ROW_KEY_PREFIX}#{key}"
      text = I18n.t(wanted)
      text unless text == wanted
    end

    # A name in words: :left_bumper is "Left bumper".
    def self.readable(name) = name.to_s.split(WORD_JOINER).join(" ").capitalize
  end
end
