# Camera behind the car, following where its nose points (asks the ArcadeCar on the target).
# Settings: target (the car: the object or its name), distance, height (m), smoothing.
class ChaseCamera < Behaviour
  DEFAULT_DISTANCE = 7.5
  DEFAULT_HEIGHT = 2.2
  DEFAULT_SMOOTHING = 5.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0

  def start
    target = prop(:target, "")
    @target = target.is_a?(GameObject) ? target : find(target)
    @car = @target&.get_component(ArcadeCar)
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @height = prop(:height, DEFAULT_HEIGHT)
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    follow(MAX_BLEND) if @car
  end

  # Stops following, leaving the camera to whoever moves it next (photo views, cutscenes).
  def release
    @car = nil
  end

  def update(dt)
    follow([@smoothing * dt, MAX_BLEND].min) if @car && @target.alive?
  end

  private

  def follow(blend)
    desired = @target.transform.position - @car.nose * @distance + Vector3.up * @height
    transform.position = transform.position.lerp(desired, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
