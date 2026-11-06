---
description: "Reload saved behaviour code during editor Play without restarting the scene."
---

# Live script reload

During editor Play, saving a loaded behaviour's file updates its methods and constants without
replacing its instances. Their fields, object identities, scene transforms and physics state
remain in place. `start` and scene setup do not run again. A removed method falls back to the
superclass, and a removed constant disappears from the class.

The editor periodically checks up to eight loaded source files; a game with many scripts
can take several polls. Reload waits while a gizmo, terrain stroke or popup is active. Typing in
a script tab does not block saved changes. **File > Reload scripts** saves open tabs and retries
all loaded behaviour files immediately, including a previously rejected version.

A reloadable file defines one top-level behaviour class, keeping its class name and superclass.
Its class body can contain instance methods, literal constants, `attr_reader`, `attr_writer`,
`attr_accessor`, and `private`/`protected`/`public` declarations. Literals include numbers,
strings, symbols, booleans, nil, arrays and hashes with string or symbol keys. Each source is
limited to 256 KiB, with bounded declaration and literal nesting checks.

```ruby
class Mover < Behaviour
  SPEED = 2.0

  def update(dt)
    @ticks = (@ticks || 0) + 1
    transform.translate(Vector3.right * SPEED * dt)
  end
end
```

New fields need a default in the running methods, as above, because existing instances keep
their fields and do not run `start` again. Methods can still call the complete scripting API.
The declaration restrictions apply to code executed while the file itself is evaluated.

Syntax errors and unsupported declarations keep the previous code running and report the
problem once per observed file version. Fix and save the file, or use **Reload scripts** to
retry. A successfully reloaded class re-enables its behaviours if an earlier callback failed.
An error in a subsequently called method follows the usual rule: that behaviour is disabled.

Stop and restart Play for executable file/class setup, computed constants, class methods,
modules/mixins, changed inheritance, custom declaration hooks or a script that failed to load
initially. Scene setup and unrelated helper files are not rerun. Existing timer blocks, event
handlers and saved method objects retain their earlier code until replaced or cancelled.
Live reload watches files only in the editor; shipped games and dedicated servers do not poll
or replace their scripts.
