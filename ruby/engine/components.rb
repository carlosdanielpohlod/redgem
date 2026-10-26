# What an object may have besides its behaviours: where it is, and its body in the physics
# world. Instances are made by GameObject, never with new; the engine defines what they do.
class Transform
  def self.__new(id, serial)
    transform = allocate
    transform.instance_variable_set(:@id, id)
    transform.instance_variable_set(:@serial, serial)
    transform
  end

  def translate(offset)
    self.position = position + offset
  end

  def inspect = "#<Transform position=#{position} rotation=#{rotation}>"
end

class CharacterController
  def self.__new(id, serial)
    controller = allocate
    controller.instance_variable_set(:@id, id)
    controller.instance_variable_set(:@serial, serial)
    controller
  end
end

# The body of a dynamic or kinematic object (and of a vehicle's chassis). Forces act during the
# next physics step only: push every fixed_update to keep pushing.
#   rigidbody.add_force(Vector3.up * 30.0)                         # newtons, at the center of mass
#   rigidbody.add_force(Vector3.right * 10.0, at: transform.position + Vector3.up)   # also turns it
#   rigidbody.kinematic = true                                      # scripts move it from now on
class Rigidbody
  def self.__new(id, serial)
    body = allocate
    body.instance_variable_set(:@id, id)
    body.instance_variable_set(:@serial, serial)
    body
  end

  def add_force(force, at: nil)
    __add_force(force, at)
  end

  def dynamic? = !kinematic?

  def dynamic=(dynamic)
    self.kinematic = !dynamic
  end
end

# The light an object shines with. Its brightness and color change from the next frame on:
#   every(0.05) { fire.light.intensity = 40 + rand * 20 }   # a flicker
#   fire.light.color = :orange                               # a name, or [r, g, b] from 0 to 255
class Light
  def self.__new(id, serial)
    light = allocate
    light.instance_variable_set(:@id, id)
    light.instance_variable_set(:@serial, serial)
    light
  end

  def color=(color)
    r, g, b = UI.rgb(color)
    __set_color(r.to_i, g.to_i, b.to_i)
  end

  def inspect = "#<Light intensity=#{intensity} color=#{color}>"
end

# A car built in Blender: `body = "vehicle"` on the chassis, `wheel = true` on its wheel children.
class Vehicle
  def self.__new(id, serial)
    vehicle = allocate
    vehicle.instance_variable_set(:@id, id)
    vehicle.instance_variable_set(:@serial, serial)
    vehicle
  end

  # throttle and steering in -1..1 (positive steering turns right), brakes in 0..1.
  # The input stays applied until the next call.
  def drive(throttle: 0.0, steering: 0.0, brake: 0.0, handbrake: 0.0)
    __drive(throttle.to_f, steering.to_f, brake.to_f, handbrake.to_f)
  end

  def stop = drive(brake: 1.0)
end
