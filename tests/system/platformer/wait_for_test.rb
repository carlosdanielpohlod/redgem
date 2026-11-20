# A wait in real seconds (t.wait_for): two seconds go by, longer than a headless run takes for
# the test's whole limit of frames, and the wait still ends in time.
REAL_SECONDS = 2.0
LIMIT_SECONDS = 10
GAME_PACE = 60 # frames a second
SLACK = 2      # the pace may slip that many times over

SystemTest.define do |t|
  started = Time.now.to_f
  t.wait_for(seconds: LIMIT_SECONDS, message: "two seconds never went by") { Time.now.to_f - started >= REAL_SECONDS }
  t.assert("the frames went at the game's pace") { Time.frame < REAL_SECONDS * GAME_PACE * SLACK }
end
