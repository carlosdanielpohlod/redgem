# Lap logic and HUD: checkpoints must be crossed in order; crossing the last one completes a lap.
class RaceManager < Behaviour
  FIRST_CHECKPOINT = 1
  HUD_X = 20
  SPEED_Y = 40
  CHECKPOINT_Y = 76
  LAP_Y = 112
  HUD_SIZE = 28
  HINT_Y = 680
  HINT_SIZE = 20
  HINT_SECONDS = 8.0
  BANNER_X = 400
  BANNER_Y = 300
  BANNER_SIZE = 56
  BANNER_SECONDS = 3.0

  attr_reader :next_checkpoint, :total_checkpoints, :laps

  def self.instance
    Scene.find("RaceManager")&.get_component(RaceManager)
  end

  def start
    @total_checkpoints = Scene.find_all(:checkpoint).size
    @next_checkpoint = FIRST_CHECKPOINT
    @laps = []
    @lap_started_at = Time.elapsed
    @banner_until = nil
    @car = Scene.find("Car")&.get_component(Car)
    highlight_next
  end

  # Called by checkpoints; out-of-order gates are ignored.
  def reach(order)
    return unless order == @next_checkpoint

    if @next_checkpoint == @total_checkpoints
      @laps << Time.elapsed - @lap_started_at
      @lap_started_at = Time.elapsed
      @banner_until = Time.elapsed + BANNER_SECONDS
      @next_checkpoint = FIRST_CHECKPOINT
    else
      @next_checkpoint += 1
    end
    highlight_next
  end

  def best_lap = @laps.min

  def update(_dt)
    UI.text(format("%3d km/h", @car ? @car.speed_kmh.abs.round : 0), x: HUD_X, y: SPEED_Y, size: HUD_SIZE, color: :yellow)
    UI.text("Checkpoint #{@next_checkpoint}/#{@total_checkpoints}", x: HUD_X, y: CHECKPOINT_Y, size: HUD_SIZE)
    UI.text(format("Lap %.1fs  Best %s", Time.elapsed - @lap_started_at, best_lap ? format("%.1fs", best_lap) : "--"),
            x: HUD_X, y: LAP_Y, size: HUD_SIZE)
    if Time.elapsed < HINT_SECONDS
      UI.text("W/S throttle, A/D steer, Space hand brake, R reset. Follow the blue banners!",
              x: HUD_X, y: HINT_Y, size: HINT_SIZE)
    end
    return unless @banner_until && Time.elapsed < @banner_until

    UI.text(format("LAP %d: %.1fs", @laps.size, @laps.last), x: BANNER_X, y: BANNER_Y, size: BANNER_SIZE,
                                                            color: :green)
  end

  private

  # Only the banner of the gate to cross next is shown.
  def highlight_next
    (FIRST_CHECKPOINT..@total_checkpoints).each do |order|
      banner = Scene.find("Checkpoint#{order}Banner")
      banner.visible = order == @next_checkpoint if banner
    end
  end
end
