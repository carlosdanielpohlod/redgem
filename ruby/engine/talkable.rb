# Someone the player can talk to: using the object (its Interactable, which this adds when it has
# none, with the prompt "Talk") starts its dialogue with the player's hero (see dialogue.rb).
#
#   find("Baker").add_component(Talkable, dialogue: "baker")
#
# Settings (each also a Blender property of the object): dialogue (the name of its file in
# dialogues/, without .json), and those of the Interactable it adds (prompt, range, priority,
# angle). In a game over the network, on_interact runs on the host, and the conversation shows on
# the machine of the player who talked.
class Talkable < Behaviour
  PROMPT = "game.dialogue.talk".freeze
  INTERACTABLE_SETTINGS = %i[prompt range priority angle].freeze

  def start
    return if get_component(Interactable)

    settings = INTERACTABLE_SETTINGS.to_h { |name| [name, prop(name)] }.compact
    game_object.add_component(Interactable, prompt: PROMPT, **settings)
  end

  # The name of its dialogue file.
  def dialogue = prop(:dialogue)&.to_s

  def on_interact(actor)
    raise ArgumentError, "#{name}'s Talkable has no dialogue: add_component(Talkable, dialogue: \"name\")" unless
      dialogue

    Dialogue.start(dialogue, actor: actor, speaker: game_object)
  end
end
