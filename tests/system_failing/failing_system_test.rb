# Deliberately failing system test: ctest expects this run to fail (WILL_FAIL).
WARMUP_FRAMES = 2

SystemTest.define do |t|
  t.wait(WARMUP_FRAMES)
  t.assert("this must fail") { Scene.find("Player").nil? }
end
