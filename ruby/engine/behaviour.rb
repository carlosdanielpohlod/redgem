# Base class for game scripts: one instance per object it is attached to.
#
#   class Player < Behaviour
#     def start; end
#     def update(dt); end
#     def fixed_update(dt); end
#     def on_trigger_enter(other); end
#     def on_trigger_exit(other); end
#     def on_collision_enter(other); end
#     def on_collision_exit(other); end
#     def on_destroy; end
#   end
class Behaviour
  attr_reader :game_object

  def start; end
  def update(_dt); end
  def fixed_update(_dt); end
  def on_trigger_enter(_other); end
  def on_trigger_exit(_other); end
  def on_collision_enter(_other); end
  def on_collision_exit(_other); end
  def on_destroy; end

  def transform = game_object.transform
  def character = game_object.character
  def rigidbody = game_object.rigidbody
  def vehicle = game_object.vehicle
  def name = game_object.name

  # Blender custom properties of this object, with symbol keys.
  def props = game_object.props

  # A setting of this behaviour: what add_component was given, else the object's Blender
  # property of that name, else the default.
  def prop(key, default = nil)
    value = @settings[key.to_sym] if @settings
    value = props[key.to_sym] if value.nil?
    value.nil? ? default : value
  end

  def find(name) = Scene.find(name)
  def find_all(tag) = Scene.find_all(tag)
  def destroy(target = game_object) = target.destroy
  def get_component(klass) = game_object.get_component(klass)

  def inspect = "#<#{self.class} on #{game_object.inspect}>"

  def __configure(settings)
    @settings = settings
  end

  def __attach(game_object)
    @game_object = game_object
    game_object.__add_behaviour(self)
  end
end
