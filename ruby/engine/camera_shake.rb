module Camera
  # The camera's shake (Camera.shake), made of trauma: each shake adds to it (never past 1), and it
  # falls to nothing over the newest shake's duration. The camera moves and turns by the square of
  # it, so a shake fades out gently, along smooth noise: the same game time always shakes the same
  # way, however long its frames are, and a test sees the same shake every run.
  class Shake
    DURATION = 0.5
    SHORTEST = 0.01            # seconds: a shake shorter than a frame would never be seen
    MOST_TRAUMA = 1.0
    MOVE = 0.15                # meters it moves along each of its own axes at full trauma
    TURN = [1.5, 1.5, 3.0].freeze # degrees it turns at full trauma: pitch, yaw, roll
    FREQUENCY = 15.0           # how many times a second the noise changes direction
    SEED = 7
    # Integer hashing (each a large odd number below 2**29, so products stay within 64 bits).
    CELL_PRIME = 73_856_093
    AXIS_PRIME = 19_349_663
    SEED_PRIME = 83_492_791
    MIX_FIRST = 0x1b873593
    MIX_SECOND = 0x0bc6ef37
    BITS = 0xffffffff
    SHIFTS = [15, 13, 16].freeze  # how far each round of the mix shifts the bits
    SPAN = 2.0                    # from 0..1 to -1..1
    MOVE_AXES = [0, 1, 2].freeze  # noise axes for moving right, up and back
    TURN_AXES = [3, 4, 5].freeze  # and for turning
    SMOOTH_START = 3.0            # smoothstep: 3t^2 - 2t^3
    SMOOTH_END = 2.0

    attr_reader :trauma

    def initialize(seed = SEED)
      @seed = seed
      stop
    end

    def stop
      @trauma = 0.0
      @fade = 0.0 # trauma lost per second
      @time = 0.0
    end

    def add(strength, duration)
      @trauma = [@trauma + strength, MOST_TRAUMA].min
      @fade = @trauma / duration
      @trauma
    end

    def active? = @trauma.positive?

    def advance(dt)
      return unless active?

      @time += dt
      @trauma = [@trauma - @fade * dt, 0.0].max
    end

    # How far the camera is off now: [meters right, up and back], [degrees of pitch, yaw and roll].
    def offset
      amount = @trauma * @trauma
      move = MOVE_AXES.map { |axis| noise(axis) * MOVE * amount }
      turn = TURN_AXES.zip(TURN).map { |axis, most| noise(axis) * most * amount }
      [Vector3.new(*move), Vector3.new(*turn)]
    end

    def noise(axis) = Shake.noise(@seed, axis, @time * FREQUENCY)

    # Smooth noise from -1 to 1 along time: random values at whole times, eased between.
    def self.noise(seed, axis, time)
      cell = time.floor
      share = time - cell
      eased = share * share * (SMOOTH_START - SMOOTH_END * share)
      from = lattice(seed, axis, cell)
      from + (lattice(seed, axis, cell + 1) - from) * eased
    end

    # A random value from -1 to 1 for a whole time, the same for the same seed, axis and time.
    def self.lattice(seed, axis, cell)
      bits = (cell * CELL_PRIME + axis * AXIS_PRIME + seed * SEED_PRIME) & BITS
      first, second, last = SHIFTS
      bits = mix(mix(bits, first, MIX_FIRST), second, MIX_SECOND)
      (bits ^ (bits >> last)).to_f / BITS * SPAN - 1.0
    end

    # One round of the hash: the high bits folded into the low ones, then spread by a multiply.
    def self.mix(bits, shift, factor) = ((bits ^ (bits >> shift)) * factor) & BITS
  end
end
