# The showcase's car, loaded by name from its scripts (redgem-test --ruby ... --game games/showcase).
class ArcadeCarTest < RedGemTest::Case
  EDGE = 10.0          # the street's half width, either way along x and z
  STEP = 0.25          # how far past (or short of) an edge a position is
  HEIGHT = 0.7         # y, which the street's edges leave alone

  def street_car
    ArcadeCar.new.tap do |car|
      %i[@min_x @min_z].each { |edge| car.instance_variable_set(edge, -EDGE) }
      %i[@max_x @max_z].each { |edge| car.instance_variable_set(edge, EDGE) }
    end
  end

  def test_a_position_on_the_street_is_not_off_it
    car = street_car
    refute car.__send__(:off_the_street?, Vector3.new(EDGE - STEP, HEIGHT, -EDGE + STEP))
    refute car.__send__(:off_the_street?, Vector3.new(EDGE, HEIGHT, EDGE))
  end

  def test_a_position_past_any_edge_is_off_the_street
    car = street_car
    assert car.__send__(:off_the_street?, Vector3.new(EDGE + STEP, HEIGHT, 0))
    assert car.__send__(:off_the_street?, Vector3.new(0, HEIGHT, -EDGE - STEP))
  end
end
