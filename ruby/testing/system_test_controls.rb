# What a system test does at the controls: the keys, the mouse and the gamepad, held for as many
# frames as the test says.
class SystemTest
  def hold(*keys, frames:)
    keys.each { |key| TestInput.set(key, true) }
    wait(frames)
    keys.each { |key| TestInput.set(key, false) }
  end

  def press(key) = hold(key, frames: 1)

  # Types a text, as on the keyboard (what Input.text and UI.text_field read).
  def type(text)
    TestInput.type(text.to_s)
    wait
    TestInput.type("")
  end

  # A gamepad: pushes a stick (:left_x, :left_y, :right_x, :right_y; left and up are negative) or
  # pulls a trigger for some frames, then lets go.
  def tilt(axis, amount, frames:)
    TestInput.set_gamepad_axis(axis, amount.to_f)
    wait(frames)
    TestInput.set_gamepad_axis(axis, 0.0)
  end

  def hold_button(*buttons, frames:)
    buttons.each { |button| TestInput.set_gamepad_button(button, true) }
    wait(frames)
    buttons.each { |button| TestInput.set_gamepad_button(button, false) }
  end

  def press_button(button) = hold_button(button, frames: 1)

  def move_mouse(x, y) = TestInput.move_mouse(x.to_f, y.to_f)

  # Moves the mouse there and clicks: down for a frame, up for the next (what UI.button waits for).
  def click(x, y, button: :left)
    move_mouse(x, y)
    wait
    TestInput.set_mouse_button(button, true)
    wait
    TestInput.set_mouse_button(button, false)
    wait
  end

  # Keeps keys down until the block is true (or fails after `frames`).
  def hold_until(*keys, frames: DEFAULT_WAIT_UNTIL_FRAMES, message: "condition not met", &condition)
    keys.each { |key| TestInput.set(key, true) }
    wait_until(frames: frames, message: message, &condition)
  ensure
    keys.each { |key| TestInput.set(key, false) }
  end
end
