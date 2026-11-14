# Something the player can use: a door, a lever, a sign, someone to talk to. The player's
# Interactor picks the best one in reach every frame, shows its prompt, and uses it when the
# :interact action is pressed (or the prompt is tapped). The object's behaviours hear of it:
#
#   find("Door").add_component(Interactable, prompt: "door.open", range: 2.5)
#
#   class Door < Behaviour
#     def on_interact(actor) = open   # actor: the object of the Interactor that used it
#     def on_focus(actor); end        # it became what the actor's prompt is for ...
#     def on_blur(actor); end         # ... and stopped being it
#   end
#
# Settings (each also a Blender property of the object, and each can change while playing):
# prompt (an I18n key; the engine's "Interact" unless said), range (meters from the actor to the
# object), priority (of those in reach, the highest wins; then the nearest), enabled (false: never
# picked), angle (degrees off the actor's forward the object may be at; any direction unless said).
# A range or an angle that is not a number from 0 up, or a priority that is not a number, raises
# as it is set (or, from a setting, as the behaviour starts).
class Interactable < Behaviour
  RANGE = 2.0
  PRIORITY = 0
  LARGEST = 1_000_000.0   # meters or degrees: past this, a range or an angle is a mistake

  attr_writer :prompt, :enabled

  # Every Interactable of the scene whose object is still there.
  def self.all = Interactable.registered.select { |interactable| interactable.game_object.alive? }

  def self.registered = @registered ||= []

  # The scene is gone, and its Interactables with it.
  def self.__forget_all = @registered = []

  # A number a setting may be: finite, and from 0 up unless it may be below.
  def self.number(value, name, below_zero: false)
    number = Float(value)
    least = below_zero ? -LARGEST : 0.0
    raise ArgumentError, "Interactable #{name} must be from #{least} to #{LARGEST}, not #{value}" unless
      number.between?(least, LARGEST)

    number
  end

  def start
    [range, priority, angle]  # a setting that is no number stops here, not in the Interactor
    Interactable.registered << self
  end

  def on_destroy = Interactable.registered.delete(self)

  # The I18n key of its prompt, or nil for the engine's own ("Interact").
  def prompt = @prompt || prop(:prompt)&.to_s

  # The prompt in the language in use.
  def prompt_text
    return I18n.t("game.interaction.interact") if prompt.nil?

    I18n.t(prompt)
  end

  def range = @range || Interactable.number(prop(:range, RANGE), :range)

  def range=(meters)
    @range = Interactable.number(meters, :range)
  end

  def priority = @priority || Interactable.number(prop(:priority, PRIORITY), :priority, below_zero: true)

  def priority=(value)
    @priority = Interactable.number(value, :priority, below_zero: true)
  end

  def enabled? = @enabled.nil? ? prop(:enabled, true) != false : @enabled

  # Degrees off the actor's forward it may be at, or nil for any direction.
  def angle
    return @angle if @angle_set

    given = prop(:angle)
    given && Interactable.number(given, :angle)
  end

  def angle=(degrees)
    @angle = degrees && Interactable.number(degrees, :angle)
    @angle_set = true
  end
end
