# A speaker's mouth that moves with its voice: as a clip of the :voice group plays (a dialogue line
# it speaks, see Dialogue::Voice, or one given to follow), the mouth opens as far as the clip is
# loud (Audio.loudness, the engine's envelope follower over the voice), through a shape key or a
# jaw bone.
#
#   baker.add_component(VoiceMouth, morph: "MouthOpen")        # a shape key, 0 to 1
#   guard.add_component(VoiceMouth, jaw: "Jaw", angle: 18.0)   # a bone that turns down
#   baker.get_component(VoiceMouth).follow(Audio.play_at(:hum, baker.transform.position, group: :voice))
#
# Settings (each also a Blender property of the object): morph (the shape key's name), jaw (the
# bone's name; a rig's prefix, as in "mixamorig:Jaw", is fine), angle (degrees the jaw turns about
# its own X axis when the mouth is all open), full (the loudness, 0 to 1, that opens it all the
# way). Without morph or jaw it moves nothing. It runs on the machine that plays the voice.
class VoiceMouth < Behaviour
  FULL = 0.25       # loudness (the clip's root mean square) that opens the mouth all the way
  ANGLE = 15.0      # degrees
  SHUT = 0.0
  OPEN = 1.0
  SAME_POSE = 0.01  # degrees: a jaw's rotation this close to what it left is still what it left

  # How open a mouth is (0 to 1) for a voice of that loudness.
  def self.openness_at(loudness, full) = (loudness / full).clamp(SHUT, OPEN)

  # How open the mouth is now (0 to 1).
  def openness = @openness || SHUT

  # Moves the mouth with a sound that plays (its handle), in place of the one it followed.
  def follow(handle)
    @handle = handle
  end

  def update(_dt)
    return if @handle.nil?

    @openness = VoiceMouth.openness_at(Audio.loudness(@handle), Float(prop(:full, FULL)))
    move_mouth
    stop_following unless Audio.playing?(@handle)
  end

  private

  def stop_following
    shut
    @handle = nil
  end

  def shut
    @openness = SHUT
    move_mouth
  end

  def move_mouth
    morph = prop(:morph)
    game_object.morph(morph, openness) if morph
    turn_jaw if prop(:jaw)
  end

  # The jaw turns from its pose: what its clips give it, or what it had before this turned it.
  def turn_jaw
    return unless jaw&.alive?

    turning = jaw.transform
    current = turning.local_rotation
    @pose = current unless @written && near?(current, @written)
    turning.local_rotation = @pose
    turning.rotate(Vector3.new(angle * openness, 0, 0))
    @written = turning.local_rotation
  end

  def angle = Float(prop(:angle, ANGLE))

  def jaw = @jaw ||= animation.bone!(prop(:jaw))

  def near?(rotation, other) = (rotation - other).length <= SAME_POSE
end
