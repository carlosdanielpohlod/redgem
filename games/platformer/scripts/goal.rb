# Finish line trigger. Wins the game once every coin is collected.
class Goal < Behaviour
  def on_trigger_enter(other)
    player = other.get_component(Player)
    return if player.nil? || player.ghost?

    GameManager.instance.reached_goal
  end
end
