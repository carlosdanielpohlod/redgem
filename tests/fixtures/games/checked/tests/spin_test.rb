# A system test that passes: the cube turns.
SystemTest.define do |t|
  t.wait(30)
  t.assert("the cube turned") { Scene.find("Cube").transform.rotation.y.abs > 1.0 }
end
