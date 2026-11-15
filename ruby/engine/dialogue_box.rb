# Shows the conversation this machine's player is in (Dialogue.current): a box at the bottom of the
# screen with who speaks and the line, which appears letter by letter, and the choices under it.
#
#   find("Player").add_component(DialogueBox)
#
# The player goes on with Enter, the :interact action (E, a gamepad's Y), or a tap or click on the
# box (not Space or a gamepad's A, which also jump): the first press shows the whole line at once, the next goes on.
# Up and down (arrows, D-pad) move among the choices; confirming takes the one in focus, and a tap
# or click takes the one under it. Backspace, a gamepad's B or the box's Skip button leaves the
# conversation. While a menu is open (Time.scale = 0) the box shows but takes no input.
#
# Settings (each also a Blender property of the object): pad (the player whose gamepad works it:
# 0 unless said), letters_per_second (how fast a line appears; 0: all at once).
class DialogueBox < Behaviour
  LETTERS_PER_SECOND = 45.0
  MARGIN = 32               # pixels from the screen's edges
  HEIGHT = 200              # pixels: the box with its line
  PADDING = 24              # pixels inside the box
  SPEAKER_SIZE = 26
  LINE_SIZE = 28
  BOX_COLOR = :dark
  BOX_ALPHA = 235
  SPEAKER_COLOR = :gold
  CHOICE_HEIGHT = 52        # pixels
  CHOICE_GAP = 10           # pixels between two choices, and between the box and the first
  CHOICE_WIDTH_SHARE = 0.6  # of the box's width
  SKIP_WIDTH = 110          # pixels
  SKIP_HEIGHT = 40
  SKIP_LABEL = "game.dialogue.skip".freeze
  # [keys, gamepad buttons] that go on, and that leave.
  GO_ON = [%i[enter], []].freeze
  LEAVE = [%i[backspace], %i[b]].freeze
  NO_CONFIRM = [[], []].freeze   # (a choice in focus is taken by listen, not by its button)

  # How many of a line's `letters` show after `seconds` at `speed` letters a second (0: all).
  def self.letters_shown(letters, seconds, speed)
    return letters unless speed.positive?

    [(seconds * speed).floor, letters].min
  end

  def update(dt)
    conversation = Dialogue.current
    return forget if conversation.nil?

    follow(conversation)
    @seconds += dt
    @focus.update(choices_count(conversation), pad: pad) if listening?(dt)
    draw(conversation)
    listen(conversation) if listening?(dt)
    @fresh = false
  end

  # Whether the line it shows has all of its letters out (true while it shows none).
  def whole? = @conversation.nil? || @whole || shown_letters >= UI.letters(line)

  def pad = prop(:pad, Input::FIRST_PAD)

  def letters_per_second = prop(:letters_per_second, LETTERS_PER_SECOND).to_f

  private

  def forget
    @conversation = nil
    @steps = nil
  end

  # A new conversation, or a new line of it (also the same line again): the line starts to appear,
  # and what was pressed on the frame it came does not count for it.
  def follow(conversation)
    return if same_line?(conversation)

    @fresh = true
    @conversation = conversation
    @steps = conversation.steps
    @seconds = 0.0
    @whole = false
    @focus = UI::Focus.new
  end

  def same_line?(conversation) = conversation.equal?(@conversation) && conversation.steps == @steps

  # The player's presses count: the game runs (not paused) and the line did not just come.
  def listening?(dt) = !dt.zero? && !@fresh

  # The line in the language shown now.
  def line = @conversation.text.to_s

  def shown_letters = DialogueBox.letters_shown(UI.letters(line), @seconds, letters_per_second)

  # The line as far as it has appeared.
  def shown_text = whole? ? line : UI.letters_of(line).first(shown_letters).join

  def draw(conversation)
    box = line_box(conversation)
    UI.rect(**box, color: BOX_COLOR, alpha: BOX_ALPHA, radius: UI::BUTTON_RADIUS)
    draw_speaker(conversation, box)
    UI.text(shown_text, x: box[:x] + PADDING, y: line_top(conversation, box), size: LINE_SIZE,
                        width: line_width(box))
    @skipped = draw_skip(box)
    @chosen = choices_shown?(conversation) ? draw_choices(conversation, box) : nil
    @tapped = UI.clicked?(UI.mouse_over?(**box))
  end

  def draw_speaker(conversation, box)
    name = conversation.speaker_name
    UI.text(name, x: box[:x] + PADDING, y: box[:y] + PADDING, size: SPEAKER_SIZE, color: SPEAKER_COLOR) if name
  end

  def line_top(conversation, box)
    top = box[:y] + PADDING
    conversation.speaker_name ? top + UI.line_height(SPEAKER_SIZE) : top
  end

  # The line keeps clear of the Skip button at the box's top right.
  def line_width(box) = box[:width] - 3 * PADDING - SKIP_WIDTH

  def draw_skip(box)
    UI.button(I18n.t(SKIP_LABEL), x: box[:x] + box[:width] - SKIP_WIDTH - PADDING, y: box[:y] + PADDING,
                                  width: SKIP_WIDTH, height: SKIP_HEIGHT, confirm: NO_CONFIRM)
  end

  # The box of the line, at the bottom of the screen, above where its choices go.
  def line_box(conversation)
    y = UI.height - MARGIN - HEIGHT - choices_count(conversation) * (CHOICE_HEIGHT + CHOICE_GAP)
    { x: MARGIN, y: y, width: UI.width - 2 * MARGIN, height: HEIGHT }
  end

  def choices_count(conversation) = choices_shown?(conversation) ? conversation.choices.size : 0

  # The choices, one under the other, below the line. The one clicked or tapped, or nil.
  def draw_choices(conversation, box)
    clicked = nil
    conversation.choices.each_with_index do |choice, row|
      clicked ||= choice if draw_choice(choice, row, box)
    end
    clicked
  end

  def draw_choice(choice, row, box)
    width = (box[:width] * CHOICE_WIDTH_SHARE).to_i
    y = box[:y] + box[:height] + CHOICE_GAP + row * (CHOICE_HEIGHT + CHOICE_GAP)
    UI.button(choice.text, x: box[:x] + box[:width] - width, y: y, width: width, height: CHOICE_HEIGHT,
                           focused: @focus.at?(row), confirm: NO_CONFIRM)
  end

  # What the player did this frame.
  def listen(conversation)
    return conversation.stop if left?
    return choose(conversation) if choices_shown?(conversation)

    go_on(conversation) if asked_on?
  end

  def choices_shown?(conversation) = whole? && conversation.choosing?

  # The first press shows the whole line; the next goes on from it.
  def go_on(conversation)
    return @whole = true unless whole?

    conversation.continue
  end

  def choose(conversation)
    chosen = @chosen || focused_choice(conversation)
    conversation.choose(chosen) if chosen
  end

  # The choice in focus, when the player confirmed it this frame.
  def focused_choice(conversation) = going_on? ? conversation.choices[@focus.index] : nil

  def left? = @skipped || UI.pressed_any?(LEAVE, pad: pad)

  # Went on by a press, or by a tap or click on the box.
  def asked_on? = going_on? || @tapped

  def going_on? = UI.pressed_any?(GO_ON, pad: pad) || Input.action_pressed?(Interaction::ACTION, pad: pad)
end
