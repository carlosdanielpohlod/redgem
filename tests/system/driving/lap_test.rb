# Gates only count in order; crossing all four completes a lap.
SETTLE_FRAMES = 30
CROSS_FRAMES = 30
GATE_HEIGHT_OFFSET = Vector3.new(0, -1, 0)
# A grass spot away from every gate.
PARKING_SPOT = Vector3.new(-30, 2, 30)
LAPS_EXPECTED = 1
FIRST_CHECKPOINT = 1

def drop_car_at(test, car, gate_name)
  car.transform.position = Scene.find(gate_name).transform.position + GATE_HEIGHT_OFFSET
  test.wait(CROSS_FRAMES)
  # Leave the gate so a later visit counts as a new entry.
  car.transform.position = PARKING_SPOT
  test.wait(CROSS_FRAMES)
end

SystemTest.define do |t|
  car = Scene.find("Car")
  race = RaceManager.instance
  t.wait(SETTLE_FRAMES)

  drop_car_at(t, car, "Checkpoint3")
  t.assert("out-of-order gate is ignored") { race.next_checkpoint == FIRST_CHECKPOINT }

  %w[Checkpoint1 Checkpoint2 Checkpoint3 Checkpoint4].each { |gate| drop_car_at(t, car, gate) }
  t.assert("one lap completed") { race.laps.size == LAPS_EXPECTED && race.laps.first.positive? }
  t.assert("next lap starts at checkpoint 1") { race.next_checkpoint == FIRST_CHECKPOINT }
end
