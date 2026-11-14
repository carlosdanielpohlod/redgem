# Counts the crates of a generated level on its first update: every one is there by then.
class CrateCounter < Behaviour
  def update(_dt)
    $crates_seen ||= Scene.find_all('crate').size
  end
end
