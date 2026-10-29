# Immediate world diagnostics: submit each frame from a behaviour's ready/update/fixed_update.
# These commands do not create objects or travel over the network. Without a presenter they
# validate their arguments and return false. Accepted commands live for this frame only.
module Debug
  def self.line(from, to, color: :green, alpha: UI::OPAQUE, depth_test: true)
    __draw_line(from, to, *UI.rgb(color), alpha, depth_test)
  end

  # Size is the full, nonnegative width/height/depth of an axis-aligned wire box.
  def self.box(center, size, color: :green, alpha: UI::OPAQUE, depth_test: true)
    __draw_box(center, size, *UI.rgb(color), alpha, depth_test)
  end
end
