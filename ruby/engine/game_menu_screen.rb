# The screens of a game's menu (see GameMenu): what each one shows, and what a game says to change it.
class GameMenu < Behaviour
  # One screen of the menu: a heading and, under it, its options as buttons. A screen that shows
  # more than that is a subclass (see what it can say about itself below).
  class Screen
    attr_writer :heading
    # What the pause keys do on it; what it changes of the menu's theme (see theme=).
    attr_reader :back, :theme_layer
    # Which of its widgets the keyboard and a gamepad work (UI::Focus): its rows first, then its
    # buttons. The menu gives it one when it shows it.
    attr_accessor :focus

    # Whether an option's extras ({id:, enabled:}) say it cannot be picked now.
    def self.disabled?(extra) = !extra[:enabled].nil? && !extra[:enabled].call

    # options: [[label, what it does (anything with `call`)], ...], each maybe with its extras
    # after them ({id: :credits, enabled: -> { ... }}: see add_option). back: what the pause keys
    # do while the screen is open (nothing unless given). A heading or a label is a text, or a
    # lookup of one (-> { I18n.t("menu.credits") }) made each time it is shown: in the language in
    # use then.
    def initialize(heading: "", options: [], back: nil)
      @heading = heading
      @options = options
      @back = back
      @theme_layer = {}
    end

    # The heading, as it is shown now.
    def heading = shown(@heading)

    # The options as they are shown now: [[label, what it does], ...].
    def options = @options.map { |label, action| [shown(label), action] }

    # What each option says of itself besides ({id:, enabled:}, maybe empty), in their order.
    def option_extras = @options.map { |option| option[2] || {} }

    # Adds an option at a place among the others (index 0 is the first button). id: its name for
    # the theme (an icon of button.icons) and a drawn button's state; enabled: a lookup that says
    # whether it can be picked now (grayed out while it cannot).
    def add_option(label, index: LAST, id: nil, enabled: nil, &action)
      @options.insert(index, [label, action, { id: id, enabled: enabled }])
    end

    # Where an option (the very [label, action] it was given) is among its options, or nil.
    def __index(option) = @options.index { |entry| entry.equal?(option) }

    # What this screen changes of the menu's theme (see UI::Theme): its own layer over it.
    def theme=(changes)
      @theme_layer = UI::Theme.read!(changes)
      UI::Theme.changed
    end

    # Draws this screen's buttons with a block (|label, box, state|), or its background.
    def on_draw_button(&block)
      @button_drawer = block
    end

    def on_draw_background(&block)
      @background_drawer = block
    end

    # What draws its buttons (its block, or its own draw_button), or its background; nil for the menu's.
    def button_drawer = @button_drawer || (->(*drawn) { draw_button(*drawn) } if respond_to?(:draw_button))
    def background_drawer = @background_drawer || (-> { draw_background } if respond_to?(:draw_background))

    def opened; end                # it was just shown
    def closed; end                # another screen took its place, or the game went on
    def draw(_first_button); end   # what else it shows; the first button is at [x, y, width, height]
    def space_above = 0            # pixels kept for that between its heading and its buttons
    def typing? = false            # letters are typed into it: of the pause keys, only Esc is one
    def rows = 0                   # widgets it draws above its buttons that take the focus, first
    def first_button_share = nil   # where its first button sits (a share of the height): the menu's unless said

    private

    def shown(text) = text.respond_to?(:call) ? text.call : text
  end
end
