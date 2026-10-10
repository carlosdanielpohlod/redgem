# Spins the car's wheels to match its speed. Works with any script on the same object that has a
# `speed` (m/s, forward positive): ArcadeCar or TrafficCar. Wheels are the children whose name
# contains "Wheel".
# Settings: wheel_radius (m), reversed (the model's nose points along -forward).
class SpinningWheels < Behaviour
  DEFAULT_RADIUS = 0.34
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  def start
    @wheels = game_object.children.select { |child| child.name.include?("Wheel") }
    @driver = game_object.behaviours.find { |behaviour| !behaviour.equal?(self) && behaviour.respond_to?(:speed) }
    @radius = prop(:wheel_radius, DEFAULT_RADIUS)
    @direction = prop(:reversed, false) ? 1.0 : -1.0
  end

  def update(dt)
    return unless @driver

    degrees = @driver.speed / @radius * DEGREES_PER_RADIAN * dt * @direction
    @wheels.each { |wheel| wheel.transform.rotate(Vector3.new(degrees, 0, 0)) }
  end
end
