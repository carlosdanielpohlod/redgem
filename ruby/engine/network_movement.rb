# Movement validation, an anti-cheat on the machine hosting the game and on a dedicated server's
# rooms: the host checks where the others say their objects are (NetworkSync) against how fast and
# how far an object of the game may move, and does not believe the moves past that. Everyone,
# the host's own scripts included, sees such an object where the host believes it is; its owner
# is told (Network::CORRECT) and puts it back there; and the violation counts against its machine,
# which is let go after kick_after of them. Off unless game.json's "movement_validation" section or
# a script turns it on:
#
#   Network.validate_movement(max_speed: 9.0, teleport_distance: 4.0, tolerance: 0.25, kick_after: 10)
#   Network.movement_violations(peer)   # how many a machine was caught at
#   Network.validate_movement           # no limits: off again
#
# A move too fast goes as far as was allowed (undo: true puts it back where it was instead); one
# longer than teleport_distance is never believed. The tolerance (a share of the limits) and a
# short burst absorb the jitter of poses arriving unevenly. It lasts until changed, across games.
module Network
  # Where an object of this machine is believed to be: [key, position].
  CORRECTION_SIZE = 2

  module Said
    class << self
      def correction?(data) = list?(data, CORRECTION_SIZE) && data[0].is_a?(String) && vector?(data[1])
    end
  end

  class << self
    # The settings the host holds the others' moves to (see the top of this file; 0: not checked).
    def validate_movement(max_speed: 0.0, teleport_distance: 0.0, tolerance: MOVEMENT_TOLERANCE, kick_after: 0,
                          undo: false)
      __validate_movement(max_speed.to_f, teleport_distance.to_f, tolerance.to_f, kick_after.to_i, undo)
    end

    private

    # Only the host corrects, and only this machine's own objects move for it.
    def receive_correction(data, from)
      @synced[data[0]]&.__corrected(data[1]) if host_correction?(data, from)
    end

    def host_correction?(data, from) = from == HOST && Said.correction?(data)
  end
end
