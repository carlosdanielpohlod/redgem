# What a car takes up on the road. Every car has one, parked ones too: whoever drives a car
# (ArcadeCar, TrafficCar) asks the others' bodies where they are and how fast they go.
# Settings: length, width (m).
class CarBody < Behaviour
  DEFAULT_LENGTH = 4.6
  DEFAULT_WIDTH = 1.9
  HALF = 0.5

  # Meters per second, in the world. Set by the car's driver; a parked car never moves.
  attr_writer :velocity

  def velocity = @velocity || Vector3.zero
  def half_length = @half_length ||= prop(:length, DEFAULT_LENGTH) * HALF
  def half_width = @half_width ||= prop(:width, DEFAULT_WIDTH) * HALF

  # The car's rectangle on the ground, where it is or where it would be at `position`.
  def footprint(position = transform.position)
    along = transform.forward.flat.normalized
    Footprint.new(x: position.x, z: position.z, along_x: along.x, along_z: along.z,
                  half_length: half_length, half_width: half_width)
  end

  # The bodies of all the other cars in the scene.
  def others
    @others ||= Scene.objects.map { |object| object.get_component(CarBody) }
                     .reject { |body| body.nil? || body.equal?(self) }
  end
end
