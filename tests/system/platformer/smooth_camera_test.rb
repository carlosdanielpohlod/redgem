# At 240 frames a second the physics still steps the hero 60 times a second, yet a camera that
# follows it moves as evenly as it runs: it follows where the hero is drawn (render_position),
# between its steps.
FRAME_RATE = 240
SETTLE_FRAMES = 30      # the hero lands
RUN_UP_FRAMES = 60      # ... and gets up to speed
MEASURED_FRAMES = 120   # half a second
EVEN_SHARE = 0.15       # how far a frame's move may be from the mean one, as a share of it
FOLLOW_OFFSET = Vector3.new(0, 5, 9)

# How far a list of positions along x moves from each one to the next.
def moves(xs) = xs.each_cons(2).map { |a, b| (b - a).abs }

def mean(values) = values.sum / values.size

SystemTest.define do |t|
  camera = Camera.main
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  # (After the game's own camera script started: the newest Camera.follow wins.)
  Camera.follow(player, offset: FOLLOW_OFFSET, smoothing: 0, avoid_walls: false)
  t.frame_rate = FRAME_RATE
  TestInput.set(:a, true)
  t.wait(RUN_UP_FRAMES)
  seen = []
  stepped = []
  MEASURED_FRAMES.times do
    t.wait
    seen << camera.transform.position.x
    stepped << player.transform.position.x
  end
  TestInput.set(:a, false)
  t.frame_rate = nil

  camera_moves = moves(seen)
  average = mean(camera_moves)
  worst = camera_moves.map { |move| (move - average).abs }.max
  still_steps = moves(stepped).count(&:zero?)
  t.log("PROBE camera move per frame m: mean #{average.round(4)}, worst off #{worst.round(4)}; " \
        "frames the hero's physics pose stood still: #{still_steps} of #{stepped.size - 1}")
  t.assert("the hero runs") { average.positive? }
  t.assert("the physics moves the hero 60 times a second, not each frame") { still_steps > stepped.size / 2 }
  t.assert("the camera moves as evenly as the hero runs") { worst <= average * EVEN_SHARE }
end
