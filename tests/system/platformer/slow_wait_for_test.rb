# A wait in real seconds (t.wait_for) on a slow machine: each of its frames takes a tenth of a
# second, so two seconds go by in about 20 frames. The wait limit holds those two real seconds
# against the test, not the 20 frames' third of a second at the game's pace.
REAL_SECONDS = 2.0
LIMIT_SECONDS = 10
SLOW_FRAME = 0.1 # real seconds each frame of the wait takes

SystemTest.define do |t|
  started = Time.now.to_f
  t.wait_for(seconds: LIMIT_SECONDS, message: "two seconds never went by") do
    frame_started = Time.now.to_f
    nil while Time.now.to_f - frame_started < SLOW_FRAME
    Time.now.to_f - started >= REAL_SECONDS
  end
end
