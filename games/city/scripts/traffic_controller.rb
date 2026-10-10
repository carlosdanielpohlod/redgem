# Alternates the green light between the avenue and the street. Blender prop: cycle (seconds).
class TrafficController < Behaviour
  DEFAULT_CYCLE = 7.0
  # All-red gap between phases so the crossroad clears.
  CLEARANCE = 1.5
  AXES = %w[avenue street].freeze

  def self.instance
    Scene.find("TrafficController")&.get_component(TrafficController)
  end

  def start
    @cycle = prop(:cycle, DEFAULT_CYCLE)
    @time = 0.0
  end

  def update(dt)
    @time += dt
  end

  # The axis whose cars may cross now, or nil during the all-red gap.
  def green_axis
    phase_time = @time % (@cycle * AXES.size)
    phase = (phase_time / @cycle).floor
    phase_time - phase * @cycle < CLEARANCE ? nil : AXES[phase]
  end

  def green?(axis) = green_axis == axis
end
