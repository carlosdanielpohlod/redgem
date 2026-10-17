# The screens of a game's menu (see GameMenu): what each one shows, and what a game says to change it.
class GameMenu < Behaviour
  # One screen of the menu: a heading and, under it, its options as buttons. A screen that shows
  # more than that is a subclass (see what it can say about itself below).
  class Screen
    attr_writer :heading
    attr_reader :back

    # options: [[label, what it does (anything with `call`)], ...]. back: what the pause keys do
    # while the screen is open (nothing unless given). A heading or a label is a text, or a lookup
    # of one (-> { I18n.t("menu.credits") }) made each time it is shown: in the language in use then.
    def initialize(heading: "", options: [], back: nil)
      @heading = heading
      @options = options
      @back = back
    end

    # The heading, as it is shown now.
    def heading = shown(@heading)

    # The options as they are shown now: [[label, what it does], ...].
    def options = @options.map { |label, action| [shown(label), action] }

    # Adds an option at a place among the others (index 0 is the first button).
    def add_option(label, index: LAST, &action) = @options.insert(index, [label, action])

    def opened; end                # it was just shown
    def closed; end                # another screen took its place, or the game went on
    def draw(_first_button); end   # what else it shows; the first button is at [x, y, width, height]
    def space_above = 0            # pixels kept for that between its heading and its buttons
    def typing? = false            # letters are typed into it: of the pause keys, only Esc is one

    private

    def shown(text) = text.respond_to?(:call) ? text.call : text
  end
end
