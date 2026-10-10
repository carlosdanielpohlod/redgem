# Keeps the score and draws the HUD. Lives on the "GameManager" empty in Blender.
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

  attr_reader :coins, :total_coins

  def self.instance
    Scene.find("GameManager")&.get_component(GameManager)
  end

  def start
    @coins = 0
    @total_coins = Scene.find_all(:coin).size
    @won = false
    @need_coins_until = nil
  end

  def collect_coin
    @coins += 1
  end

  def all_coins? = @coins >= @total_coins
  def won? = @won

  def reached_goal
    if all_coins?
      @won = true
    else
      @need_coins_until = Time.elapsed + NEED_COINS_SECONDS
    end
  end

  def update(_dt)
    UI.text("Coins #{@coins}/#{@total_coins}", x: HUD_X, y: COINS_Y, size: HUD_SIZE, color: :gold)
    if Time.elapsed < HINT_SECONDS
      UI.text("WASD / arrows to move, Space to jump. Grab every coin, then reach the flag!",
              x: HUD_X, y: HINT_Y, size: HINT_SIZE)
    end
    if @won
      UI.text("YOU WIN!", x: BANNER_X, y: BANNER_Y, size: BANNER_SIZE, color: :yellow)
    elsif @need_coins_until && Time.elapsed < @need_coins_until
      UI.text("Collect all the coins first!", x: BANNER_X, y: BANNER_Y, size: HUD_SIZE, color: :orange)
    end
  end
end
