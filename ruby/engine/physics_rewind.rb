# Lag compensation, on the host: a shot judged against what its shooter saw. A guest shows the
# others a moment behind the host (their poses come through it, and are shown a little behind the
# newest), and every message it sends carries that moment, its view time. In the block, every
# object the network keeps in step (NetworkSync), with every body that hangs from it (the hit boxes
# of Hitboxes.build too), is where it was shown at that time; after it, back where it is.
#
#   Network.on(:fire) do |shot, from, view_time|
#     hit = Physics.rewind(view_time) { Physics.raycast(Vector3[*shot[:from]], Vector3[*shot[:aim]], 100) }
#     damage(hit) if hit
#   end
#
# The time is clamped: never after now, nor further back than Physics.max_rewind seconds
# (game.json's "network": {"max_rewind_ms": 200}), so a shooter far away cannot shoot at where the
# others were long ago. nil, or not on the host, runs the block as things are.
module Physics
  # Runs the block with the world as it was at `time` (on the host's clock: Network.time); returns
  # what the block does.
  def self.rewind(time)
    raise ArgumentError, "Physics.rewind takes a block" unless block_given?
    return yield if time.nil? || !__rewind(time.to_f)

    begin
      yield
    ensure
      __restore
    end
  end
end
