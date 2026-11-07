# What the menu's controls page (see GameMenu::ControlsScreen) calls things, in the language in
# use: a binding ("key:space" is "Space", "mouse:left" "Left mouse button", "pad:a" "A button"), a
# stick (:left_x is "Left stick X")
# and a row (the action :jump is "Jump", the negative side of :horizontal "Move left").
class GameMenu < Behaviour
  module BindingNames
    # Keys with a word for a name. Letters and function keys show as capitals ("K", "F5"), digits
    # as they are.
    KEYS = { space: -> { I18n.t("game.controls.key_space") },
             escape: -> { I18n.t("game.controls.key_escape") },
             enter: -> { I18n.t("game.controls.key_enter") },
             tab: -> { I18n.t("game.controls.key_tab") },
             backspace: -> { I18n.t("game.controls.key_backspace") },
             insert: -> { I18n.t("game.controls.key_insert") },
             delete: -> { I18n.t("game.controls.key_delete") },
             home: -> { I18n.t("game.controls.key_home") },
             end: -> { I18n.t("game.controls.key_end") },
             page_up: -> { I18n.t("game.controls.key_page_up") },
             page_down: -> { I18n.t("game.controls.key_page_down") },
             up: -> { I18n.t("game.controls.key_up") },
             down: -> { I18n.t("game.controls.key_down") },
             left: -> { I18n.t("game.controls.key_left") },
             right: -> { I18n.t("game.controls.key_right") },
             caps_lock: -> { I18n.t("game.controls.key_caps_lock") },
             scroll_lock: -> { I18n.t("game.controls.key_scroll_lock") },
             num_lock: -> { I18n.t("game.controls.key_num_lock") },
             print_screen: -> { I18n.t("game.controls.key_print_screen") },
             pause: -> { I18n.t("game.controls.key_pause") },
             menu: -> { I18n.t("game.controls.key_menu") },
             left_shift: -> { I18n.t("game.controls.key_left_shift") },
             right_shift: -> { I18n.t("game.controls.key_right_shift") },
             left_control: -> { I18n.t("game.controls.key_left_control") },
             right_control: -> { I18n.t("game.controls.key_right_control") },
             left_alt: -> { I18n.t("game.controls.key_left_alt") },
             right_alt: -> { I18n.t("game.controls.key_right_alt") },
             left_super: -> { I18n.t("game.controls.key_left_super") },
             right_super: -> { I18n.t("game.controls.key_right_super") },
             kp_enter: -> { I18n.t("game.controls.key_kp_enter") } }.freeze
    # Keys shown as the character they type.
    CHARACTERS = { apostrophe: "'", comma: ",", minus: "-", period: ".", slash: "/", semicolon: ";", equals: "=",
                   left_bracket: "[", right_bracket: "]", backslash: "\\", grave: "`" }.freeze
    # The keypad's keys, shown as "Numpad" and what they type (its digits by their names' ends).
    KEYPAD_PREFIX = "kp_".freeze
    KEYPAD_CHARACTERS = { kp_decimal: ".", kp_divide: "/", kp_multiply: "*", kp_subtract: "-", kp_add: "+",
                          kp_equals: "=" }.freeze
    MOUSE = { left: -> { I18n.t("game.controls.mouse_left") },
              right: -> { I18n.t("game.controls.mouse_right") },
              middle: -> { I18n.t("game.controls.mouse_middle") },
              side: -> { I18n.t("game.controls.mouse_side") },
              extra: -> { I18n.t("game.controls.mouse_extra") },
              wheel_up: -> { I18n.t("game.controls.mouse_wheel_up") },
              wheel_down: -> { I18n.t("game.controls.mouse_wheel_down") } }.freeze
    # What names each kind of binding (a key, unless it says otherwise).
    KINDS = { Input::PAD_PREFIX => :button, Input::MOUSE_PREFIX => :mouse }.freeze
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

    # A binding by name: "key:left_shift" is "Left Shift", "mouse:left" "Left mouse button".
    def self.of(binding)
      kind, name = binding.split(":", 2)
      send(KINDS.fetch("#{kind}:", :key), name.to_sym)
    end

    def self.key(name) = KEYS[name]&.call || CHARACTERS[name] || keypad(name) || name.to_s.upcase
    def self.mouse(name) = MOUSE[name]&.call || readable(name)

    # A keypad key's name ("Numpad 1", "Numpad +"), or nil for another key.
    def self.keypad(name)
      text = name.to_s
      return unless text.start_with?(KEYPAD_PREFIX)

      I18n.t("game.controls.key_keypad", key: KEYPAD_CHARACTERS[name] || text.delete_prefix(KEYPAD_PREFIX))
    end

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
