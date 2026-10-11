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

  HOME = "RaceManager".freeze   # the object it lives on
  CAR = "Car".freeze
  CHECKPOINT_TAG = :checkpoint
  NO_LAP_YET = "--".freeze      # shown as the best lap before one is finished

  attr_reader :next_checkpoint, :total_checkpoints, :laps

  def self.instance = Scene.find!(HOME).get_component(RaceManager)

  def start
    @total_checkpoints = Scene.find_all(CHECKPOINT_TAG).size
    @next_checkpoint = FIRST_CHECKPOINT
    @laps = []
    @lap_started_at = Time.elapsed
    @banner_until = 0.0
    @car = Scene.find!(CAR).get_component(Car)
    highlight_next
  end

  # Called by checkpoints; out-of-order gates are ignored.
  def reach(order)
    return unless order == @next_checkpoint

    if order == @total_checkpoints
      finish_lap
    else
      @next_checkpoint += 1
    end
    highlight_next
  end

  def best_lap = @laps.min

  def update(_dt)
    draw_speed
    UI.text("Checkpoint #{@next_checkpoint}/#{@total_checkpoints}", x: HUD_X, y: CHECKPOINT_Y, size: HUD_SIZE)
    draw_lap_times
    draw_hint if Time.elapsed < HINT_SECONDS
    draw_lap_banner if Time.elapsed < @banner_until
  end

  private

  def finish_lap
    @laps << Time.elapsed - @lap_started_at
    @lap_started_at = Time.elapsed
    @banner_until = Time.elapsed + BANNER_SECONDS
    @next_checkpoint = FIRST_CHECKPOINT
  end

  def draw_speed
    speed = format("%<kmh>3d km/h", kmh: @car.speed_kmh.abs.round)
    UI.text(speed, x: HUD_X, y: SPEED_Y, size: HUD_SIZE, color: :yellow)
  end

  def draw_lap_times
    best = best_lap ? format("%<seconds>.1fs", seconds: best_lap) : NO_LAP_YET
    times = format("Lap %<seconds>.1fs  Best %<best>s", seconds: Time.elapsed - @lap_started_at, best: best)
    UI.text(times, x: HUD_X, y: LAP_Y, size: HUD_SIZE)
  end

  def draw_hint
    UI.text("W/S throttle, A/D steer, Space hand brake, R reset. Follow the blue banners!",
            x: HUD_X, y: HINT_Y, size: HINT_SIZE)
  end

  def draw_lap_banner
    lap = format("LAP %<number>d: %<seconds>.1fs", number: @laps.size, seconds: @laps.last)
    UI.text(lap, x: BANNER_X, y: BANNER_Y, size: BANNER_SIZE, color: :green)
  end

  # Only the banner of the gate to cross next is shown.
  def highlight_next
    (FIRST_CHECKPOINT..@total_checkpoints).each do |order|
      banner = Scene.find("Checkpoint#{order}Banner")
      banner.visible = order == @next_checkpoint if banner
    end
  end
end
