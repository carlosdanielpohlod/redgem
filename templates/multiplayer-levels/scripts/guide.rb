# Someone to talk to (custom property "dialogue": the file in dialogues/, "guide" unless said): a
# player walks up and presses E. The engine's Talkable shows the prompt and starts the
# conversation, and the dialogue's choices start the quest (quests/beacon.json). On each machine
# the guide turns to that machine's hero.
class Guide < Behaviour
  DIALOGUE = "guide".freeze
  HERO = "Player".freeze
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  def start
    game_object.add_component(Talkable, dialogue: prop(:dialogue, DIALOGUE))
    @hero = find(HERO)
  end

  def update(_dt)
    face(@hero.transform.position) if @hero&.alive?
  end

  private

  def face(point)
    way = point - transform.position
    transform.rotation = Vector3.new(0, Math.atan2(-way.x, -way.z) * DEGREES_PER_RADIAN, 0)
  end
end
