# The scene's animation clips, by the name they have in Blender. All of them play in a loop from
# the start; this is for the ones that should not: a door that opens once, a lift that waits.
#
#   Animation.stop(:DoorOpen)                 # in a scene script's setup: the door starts closed
#   Animation.play(:DoorOpen, loop: false)    # opens and stays open
#   Animation.play(:DoorOpen, loop: false, speed: -1.0)   # closes
module Animation
  NORMAL_SPEED = 1.0

  # Carries on from where the clip is; restart: true plays it from the beginning.
  def self.play(name, loop: true, speed: NORMAL_SPEED, restart: false)
    __play(name, loop, speed.to_f, restart)
  end
end
