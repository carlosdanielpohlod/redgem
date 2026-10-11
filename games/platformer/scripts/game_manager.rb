# Keeps the score, times the run and draws the HUD; the fastest win is kept between runs (Save).
# Lives on the "GameManager" empty in Blender.
class GameManager < Behaviour
  HUD_X = 20
  COINS_Y = 40
  HINT_Y = 76
  HUD_SIZE = 28
  HINT_SIZE = 20
  HINT_SECONDS = 8.0
  BANNER_X = 430
  BANNER_Y = 300
  BANNER_SIZE = 64
  NEED_COINS_SECONDS = 2.5
  BEST_TIME_KEY = :best_time
  BEST_RIGHT_MARGIN = 20   # pixels between the best time and the screen's right edge

  attr_reader :coins, :total_coins, :best_time

  def self.instance
    Scene.find("GameManager")&.get_component(GameManager)
  end

  def start
    @coins = 0
    @total_coins = Scene.find_all(:coin).size
    @won = false
    @need_coins_until = nil
    @started_at = Time.elapsed
    @best_time = Save[BEST_TIME_KEY]
  end

  # Seconds played in this run (the clock stops while a menu is open, and when the game is won).
  def run_time = (@finished_at || Time.elapsed) - @started_at

  def collect_coin
    @coins += 1
  end

  def all_coins? = @coins >= @total_coins
  def won? = @won

  def reached_goal
    if all_coins?
      @won = true
      @finished_at = Time.elapsed
      record = @best_time.nil? || run_time < @best_time
      Save[BEST_TIME_KEY] = @best_time = run_time if record
      GameMenu.instance&.finish(record ? format("New record: %.1f s!", run_time) : "You win!")
    else
      @need_coins_until = Time.elapsed + NEED_COINS_SECONDS
    end
  end

  def update(_dt)
    UI.text("Coins #{@coins}/#{@total_coins}", x: HUD_X, y: COINS_Y, size: HUD_SIZE, color: :gold)
    if @best_time
      UI.text(format("Best %.1f s", @best_time), x: UI.width - BEST_RIGHT_MARGIN, y: COINS_Y, size: HUD_SIZE,
              align: :right)
    end
    if run_time < HINT_SECONDS
      UI.text("WASD / arrows or a gamepad to move, Space or A to jump. Grab every coin, then reach the flag!",
              x: HUD_X, y: HINT_Y, size: HINT_SIZE)
    end
    if @won
      UI.text("YOU WIN!", x: BANNER_X, y: BANNER_Y, size: BANNER_SIZE, color: :yellow)
    elsif @need_coins_until && Time.elapsed < @need_coins_until
      UI.text("Collect all the coins first!", x: BANNER_X, y: BANNER_Y, size: HUD_SIZE, color: :orange)
    end
  end
end
