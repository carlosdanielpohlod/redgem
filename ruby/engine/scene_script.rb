# Sets a scene up from Ruby when it loads: which behaviours run on which objects, and with what
# settings. The engine looks for scripts/<scene>_scene.rb ("main" is the game's scene.glb) and
# calls `setup` once the scene's objects exist, before any behaviour starts.
#
#   class MainScene < SceneScript
#     def setup
#       find("Player").add_component(Player, speed: 6.0)
#       find_all("coin").each { |coin| coin.add_component(Coin) }
#     end
#   end
#
# Blender says what is in the scene and where; this says what it does. Behaviours can still be
# attached in Blender with the `script` property: both ways work together.
class SceneScript
  def setup; end

  def find(name) = Scene.find(name)
  def find_all(tag) = Scene.find_all(tag)

  # Like find, for objects the scene cannot work without.
  def find!(name) = Scene.find!(name, wanted_by: self.class)

  def self.__run(klass)
    raise TypeError, "#{klass} must inherit from SceneScript" unless klass <= SceneScript

    klass.new.setup
  end
end
