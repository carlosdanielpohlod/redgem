# A test that plays walking to a spot: what a click on the ground does (walk_to, given the spot the
# mouse is over). Something in the way stops the walk, and so do the keys.
SETTLE_FRAMES = 30           # the hero lands on the ground first
ARRIVE_FRAMES = 240          # four seconds
TURN_BACK_FRAMES = 5
SPOT = Vector3.new(3, 0, 4)  # a spot of the ground with nothing in the way
ROCK_SPOT = Vector3.new(-4, 0, 4) # the middle of Rock2, which the hero cannot walk into
FAR_SPOT = Vector3.new(-3, 0, -3)

SystemTest.define do |t|
  hero = Scene.find!("Player")
  player = hero.get_component(Player)
  t.wait(SETTLE_FRAMES)

  player.walk_to(SPOT)
  t.wait_until(frames: ARRIVE_FRAMES, message: "the hero did not get to the spot") { player.destination.nil? }
  t.assert("the hero stands on the spot") { hero.transform.position.flat.distance_to(SPOT) <= Player::ARRIVED }

  player.walk_to(ROCK_SPOT)
  t.wait_until(frames: ARRIVE_FRAMES, message: "the hero never gave up on the rock") { player.destination.nil? }
  t.assert("it gave up in front of the rock") { hero.transform.position.flat.distance_to(ROCK_SPOT) > Player::ARRIVED }

  player.walk_to(FAR_SPOT)
  t.hold(:s, frames: TURN_BACK_FRAMES)
  t.assert("a key takes over from the walk to the spot") { player.destination.nil? }
end
