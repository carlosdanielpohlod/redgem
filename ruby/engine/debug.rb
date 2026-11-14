# Immediate world diagnostics: submit each frame from a behaviour's ready/update/fixed_update.
# These commands do not create objects or travel over the network. Without a presenter they
# validate their arguments and return false. Accepted commands live for this frame only.
module Debug
  LABEL_SIZE = 16 # pixels: the height of a label's text unless given

  def self.line(from, to, color: :green, alpha: UI::OPAQUE, depth_test: true)
    __draw_line(from, to, *UI.rgb(color), alpha, depth_test)
  end

  # Size is the full, nonnegative width/height/depth of an axis-aligned wire box.
  def self.box(center, size, color: :green, alpha: UI::OPAQUE, depth_test: true)
    __draw_box(center, size, *UI.rgb(color), alpha, depth_test)
  end

  # Text centered where the main camera shows a world point, over the game's interface. Nothing is
  # shown for a point behind the camera, or in a scene without a main camera.
  def self.label(text, at:, color: :white, alpha: UI::OPAQUE, size: LABEL_SIZE)
    __draw_label(text.to_s, at, *UI.rgb(color), alpha, size)
  end
end
