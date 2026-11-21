# Where each player's hero starts a level: the objects tagged "spawn" (Spawn1, Spawn2...), by name.
# Each machine puts its own hero on its own point: the host's hero (or the one playing alone) on
# Spawn1, the next player's on Spawn2, and so on around. Add more points for more players.
module Spawns
  TAG = :spawn
  FIRST_ID = 1   # the host's id; offline, this machine counts as the host
  FEET = 1.2     # how far above a point a hero's middle stands (its capsule is 2 meters tall)

  # Which point (from 0) the machine of that id gets, of `count`.
  def self.index(id, count) = (id - FIRST_ID) % count

  # The point of this machine's hero.
  def self.mine
    points = Scene.find_all(TAG).sort_by(&:name)
    id = Network.online? ? Network.id : FIRST_ID
    points[index(id, points.size)]
  end

  # Puts a hero on this machine's point, standing up.
  def self.place(hero)
    point = mine
    hero.transform.position = point.transform.position + Vector3.new(0, FEET, 0) if point
  end
end
