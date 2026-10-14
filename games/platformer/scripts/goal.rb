# Finish line trigger. Wins the game once every coin is collected.
class Goal < Behaviour
  def on_trigger_enter(other)
    return unless Player.own?(other)

    GameManager.instance.reached_goal
  end
end
