# Child object: on_destroy must still see its parent when both are destroyed together.
class Kid < Behaviour
  def on_destroy
    $kid_saw_parent = game_object.parent&.name
    $kid_saw_family = game_object.parent&.get_component(Family)&.class&.to_s
  end
end
