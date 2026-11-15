# Dialogue::Voice: the clip of a line's "voice" (see dialogue.rb), played as the line is shown to
# this machine's player. It plays in the :voice group (the player's voice volume applies, and the
# music ducks under it: Audio.ducking), from the speaker's head as the speaker moves: its HeadLook's
# bone, or the bone called "Head", or the speaker itself when it has neither. A conversation with no
# speaker hears it with no place in the world. A speaker with a VoiceMouth moves its mouth with it.
module Dialogue
  class Voice
    GROUP = :voice

    # The voice of a line, playing; nil when the clip does not play (no such sound): the line then
    # waits for the player, as one with no voice does.
    def self.play(name, speaker)
      voice = new(name, speaker)
      voice.playing? ? voice : nil
    end

    attr_reader :handle

    def initialize(name, speaker)
      @speaker = speaker if speaker&.alive?
      @head = head_of(@speaker) if @speaker
      @handle = start(name)
      mouth&.follow(@handle)
    end

    def playing? = Audio.playing?(@handle)

    # Moves the clip to where the speaker's head is now.
    def follow
      Audio.move(@handle, place) if @speaker&.alive?
    end

    def stop = Audio.stop(@handle)

    # Where the clip plays from: the speaker's head, or the speaker.
    def place = (@head&.alive? ? @head : @speaker).transform.position

    private

    def mouth = @speaker&.get_component(VoiceMouth)

    def start(name) = @speaker ? Audio.play_at(name, place, group: GROUP) : Audio.play(name, group: GROUP)

    def head_of(speaker) = speaker.get_component(HeadLook)&.bone || speaker.animation.bone(Animator::HEAD)
  end
end
