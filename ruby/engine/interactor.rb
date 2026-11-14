# Lets the object it is on (the player's hero) use what is near it. Every frame it picks the best
# Interactable in reach (Interaction.best_for), tells it when it gains and loses the focus, shows
# its prompt at the bottom of the screen with what the :interact action is bound to ("[E] Open
# the door", "[Y button] ..." with a gamepad), and uses it when the action is pressed or the
# prompt is tapped (or clicked).
#
#   find("Player").add_component(Interactor)
#
# Settings: pad (the player whose :interact it reads: 0 unless said), prompt (false draws no
# prompt: the game draws its own from `target`). While a menu is open (Time.scale = 0) it neither
# looks nor shows. On another machine's hero, and on a dedicated server, it does nothing.
class Interactor < Behaviour
  PROMPT_HEIGHT = 52      # pixels
  PROMPT_PADDING = 24     # pixels beside the prompt's text
  PROMPT_MARGIN = 96      # pixels from the bottom of the screen
  PROMPT_ANCHOR = :bottom

  # The Interactable in focus, or nil.
  attr_reader :target

  def update(dt)
    return refocus(nil) unless played?
    return if dt.zero?  # a menu is open: the focus waits for it to close

    refocus(Interaction.best_for(game_object))
    use if @target && asked?
  end

  # Uses what is in focus now (see Interaction.interact). False when nothing is.
  def use = @target ? Interaction.interact(game_object, @target) : false

  def on_destroy = refocus(nil)

  def pad = prop(:pad, Input::FIRST_PAD)

  # What the prompt says for a target: "[E] Talk", or the target's text alone when the action is
  # bound to nothing.
  def prompt_label(target)
    binding = binding_shown
    text = target.prompt_text
    binding ? I18n.t("game.interaction.prompt", binding: binding, prompt: text) : text
  end

  private

  # This machine plays the object: not another's hero, nor a dedicated server's stand-in.
  def played? = !Network.server? && get_component(NetworkSync)&.mine? != false

  def prompt? = prop(:prompt, true) != false

  # Whether the player asked to use the target this frame: by the action, or by tapping the prompt
  # (drawn either way).
  def asked?
    tapped = prompt? && draw_prompt
    tapped || Input.action_pressed?(Interaction::ACTION, pad: pad)
  end

  def refocus(best)
    return if best.equal?(@target)

    Interaction.__blurred(game_object, @target) if still_there?(@target)
    @target = best
    Interaction.__focused(game_object, best) if best
  end

  # A target that was not destroyed meanwhile (a destroyed one hears nothing more).
  def still_there?(target) = !target.nil? && target.game_object.alive?

  # Draws the prompt; true when it was tapped.
  def draw_prompt
    label = prompt_label(@target)
    width = UI.text_width(label, size: UI.label_size(PROMPT_HEIGHT)) + 2 * PROMPT_PADDING
    UI.button(label, anchor: PROMPT_ANCHOR, x: 0, y: -PROMPT_MARGIN, width: width, height: PROMPT_HEIGHT)
  end

  # The name of the :interact binding the player would reach for: a gamepad's while one is
  # plugged in, a key's or the mouse's otherwise; nil when nothing is bound.
  def binding_shown
    bindings = Input.bindings(Interaction::ACTION, pad: pad)
    pad_first = Input.gamepad?(pad: pad)
    chosen = bindings.find { |binding| binding.start_with?(Input::PAD_PREFIX) == pad_first } || bindings.first
    chosen && GameMenu::BindingNames.of(chosen)
  end
end
