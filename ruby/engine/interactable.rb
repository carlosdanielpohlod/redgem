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
class Interactable < Behaviour
  RANGE = 2.0
  PRIORITY = 0

  attr_writer :prompt, :range, :priority, :enabled

  # Every Interactable of the scene whose object is still there.
  def self.all = registered.select { |interactable| interactable.game_object.alive? }

  def self.registered = @registered ||= []

  # The scene is gone, and its Interactables with it.
  def self.__forget_all = @registered = []

  def start = Interactable.registered << self
  def on_destroy = Interactable.registered.delete(self)

  # The I18n key of its prompt, or nil for the engine's own ("Interact").
  def prompt = @prompt || prop(:prompt)&.to_s

  # The prompt in the language in use.
  def prompt_text
    return I18n.t("game.interaction.interact") if prompt.nil?

    I18n.t(prompt)
  end

  def range = Float(@range || prop(:range, RANGE))
  def priority = Float(@priority || prop(:priority, PRIORITY))
  def enabled? = @enabled.nil? ? prop(:enabled, true) != false : @enabled

  # Degrees off the actor's forward it may be at, or nil for any direction.
  def angle = @angle_set ? @angle : prop(:angle)

  def angle=(degrees)
    @angle_set = true
    @angle = degrees
  end
end
