# Finish line trigger. Wins the game once every coin is collected.
class Goal < Behaviour
  def on_trigger_enter(other)
    return unless other.get_component(Player)

    GameManager.instance&.reached_goal
  end
end
