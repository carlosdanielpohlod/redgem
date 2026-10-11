# Cars on the showcase street never drive through each other: traffic follows the car in front,
# goes around a standing car when nobody is coming, stops behind it when there is no room, and the
# player's car bounces off what it hits.
CAR_NAME = "Generic Sport Sedan 2022 PL (placed)".freeze
FOLLOWER = "TrafficWhite".freeze   # faster ...
LEADER = "TrafficBlue".freeze      # ... than the car it shares a lane with
LANE_X = -1.2                   # the middle of that lane (it drives toward +Z)
# Where the other lane's cars (driving toward -Z) are sent meanwhile: already past the leader.
ONCOMING = { "TrafficGrey" => -175.0, "TrafficGreen" => -165.0 }.freeze
SAME_WAY = "TrafficGreen".freeze   # the last car of the player's lane, slower than the player can go
SETTLE_FRAMES = 10
STOP_FRAMES = 90                # of hard braking: from top speed to a stop
MAX_OVERLAP = 0.05              # meters two cars may sink into each other for a frame
FOLLOW_DISTANCE = 14.0          # meters between centers when the follower is dropped behind the leader
FOLLOW_FRAMES = 200
SPEED_TOLERANCE = 0.6           # m/s a follower may differ from the car it follows
STANDING_Z = -110.0             # the player stands in the lane here ...
APPROACH_Z = -150.0             # ... and the leader comes from here
PASS_FRAMES = 600
MIN_MOVED_OVER = 2.0            # meters across the road to get around the player's car
LANE_TOLERANCE = 0.05
BLOCK_Z = -47.0                 # beside a parked car: with the player in the middle, nobody gets through
BLOCK_FRAMES = 1500             # long enough for a car anywhere in the lane to come around
LANE_CARS = %w[TrafficBlue TrafficWhite TrafficBeige].freeze
STOPPED = 0.2                   # m/s
RAM_Z = -20.0
RAM_DISTANCE = 25.0             # meters behind the slower car when the player floors it
RAM_FRAMES = 360
SQUARE_SIDE = 2.0
FAR_APART = 10.0
TOUCHING = 1.5
QUARTER = 0.25

def square(x, z, along_x = 0.0, along_z = 1.0)
  half = SQUARE_SIDE / 2
  Footprint.new(x: x, z: z, along_x: along_x, along_z: along_z, half_length: half, half_width: half)
end

SystemTest.define do |t|
  # The rectangles the cars are made of.
  t.assert("rectangles apart do not touch") { !square(0, 0).touches?(square(FAR_APART, 0)) }
  t.assert("overlapping rectangles are pushed apart the short way") do
    push = square(0, 0).push_out_of(square(TOUCHING, 0))
    (push[0] + (SQUARE_SIDE - TOUCHING)).abs < MAX_OVERLAP && push[1].abs < MAX_OVERLAP
  end
  t.assert("a turned rectangle reaches further across") do
    diagonal = Math.sqrt(QUARTER * 2)  # a square turned 45 degrees
    turned = square(0, 0, diagonal, diagonal)
    reaches = (turned.half_extent_x - SQUARE_SIDE * diagonal).abs < MAX_OVERLAP
    reaches && turned.touches?(square(SQUARE_SIDE + QUARTER, 0))
  end

  bodies = Scene.objects.map { |object| object.get_component(CarBody) }.compact
  body_of = ->(name) { Scene.find(name).get_component(CarBody) }
  t.assert("the player's car, the traffic and the parked cars have bodies") { bodies.size >= 6 }
  # How deep the worst pair of cars is into each other right now.
  deepest = lambda do
    bodies.combination(2).map do |a, b|
      push = a.footprint.push_out_of(b.footprint)
      [push ? Math.sqrt(push[0]**2 + push[1]**2) : 0.0, "#{a.name} and #{b.name}"]
    end.max_by(&:first)
  end
  overlap = -> { deepest.call.first }
  worst = [0.0, "no cars"]
  watch = lambda do |frames, keys = [], &each_frame|
    frames.times do |frame|
      keys.empty? ? t.wait(1) : t.hold(*keys, frames: 1)
      now = deepest.call
      worst = now + [frame] if now.first > worst.first
      each_frame&.call
    end
  end
  gap = lambda do |behind, ahead|
    (behind.transform.position.z - ahead.transform.position.z).abs - behind.half_length - ahead.half_length
  end

  car = Scene.find(CAR_NAME)
  follower = Scene.find(FOLLOWER)
  leader = Scene.find(LEADER)
  t.wait(SETTLE_FRAMES)
  t.assert("no car starts inside another") { overlap.call.zero? }

  # A faster car dropped right behind a slower one settles at its speed.
  t.assert("traffic starts in its lane") { (leader.transform.position.x - LANE_X).abs < LANE_TOLERANCE }
  follower.transform.position = leader.transform.position - Vector3.new(0, 0, FOLLOW_DISTANCE)
  watch.call(FOLLOW_FRAMES)
  t.assert("traffic follows the car in front at its speed") do
    (follower.get_component(TrafficCar).speed - leader.get_component(TrafficCar).speed).abs < SPEED_TOLERANCE &&
      gap.call(body_of.call(FOLLOWER), body_of.call(LEADER)).positive?
  end

  # The player stands in their lane while nobody comes the other way: they go around.
  height = car.transform.position.y
  ONCOMING.each { |name, z| Scene.find(name).transform.position = Scene.find(name).transform.position.with(z: z) }
  car.transform.position = Vector3.new(LANE_X, height, STANDING_Z)
  leader.transform.position = leader.transform.position.with(x: LANE_X, z: APPROACH_Z)
  moved_over = 0.0
  watch.call(PASS_FRAMES) { moved_over = [moved_over, leader.transform.position.x - LANE_X].max }
  t.screenshot("showcase_traffic")
  t.assert("traffic goes around a standing car when the other lane is free") { moved_over > MIN_MOVED_OVER }
  t.assert("... and returns to its lane") { (leader.transform.position.x - LANE_X).abs < LANE_TOLERANCE }
  t.assert("... having passed it") { leader.transform.position.z > STANDING_Z }

  # The player floors it into the back of a slower car going the same way (toward -Z).
  slower = Scene.find(SAME_WAY)
  driver = car.get_component(ArcadeCar)
  slower.transform.position = slower.transform.position.with(z: RAM_Z)  # far from the end of the street
  car.transform.position = slower.transform.position.with(y: height) + Vector3.new(0, 0, RAM_DISTANCE)
  bounced = false
  sparks = 0
  watch.call(RAM_FRAMES, [:w]) do
    bounced ||= driver.speed.negative?
    sparks = [sparks, Particles.count].max
  end
  t.assert("the player's car bounces off the car it hits") { bounced }
  t.assert("... in a shower of sparks") { sparks.positive? }
  t.assert("... and does not get through it") { car.transform.position.z > slower.transform.position.z }

  # The player stops in the middle of the road beside a parked car: no room on either side. Whoever
  # comes down the lane next has to wait.
  t.hold(:space, frames: STOP_FRAMES)
  car.transform.position = Vector3.new(0, height, BLOCK_Z)
  lane = LANE_CARS.map { |name| Scene.find(name) }
  waiting = nil
  watch.call(BLOCK_FRAMES) do
    waiting ||= lane.find do |other|
      other.get_component(TrafficCar).speed < STOPPED && other.transform.position.z < BLOCK_Z
    end
  end
  t.assert("traffic stops behind a car it cannot get around") do
    waiting && gap.call(waiting.get_component(CarBody), body_of.call(CAR_NAME)).positive? &&
      waiting.get_component(TrafficCar).speed < STOPPED
  end
  t.assert("cars never overlap (worst: #{worst.first.round(3)} m, #{worst[1]}, frame #{worst[2]} of its scenario)") do
    worst.first < MAX_OVERLAP
  end
end
