# Objects a game spawned as it ran (dropped items, placed buildings), kept in the save with a
# checkpoint. Only copies whose behaviours ask for it are: a behaviour with `persist :field, ...`
# (see Behaviour.persist). A checkpoint keeps each copy's template, name, pose and look (its
# capture_state) and the persisted fields of its behaviours; restoring it spawns them again.
#
#   Save.checkpoint                 # in the scene, whenever the game saves
#   Save.restore_checkpoint         # after the scene loads (its SceneScript#setup, say)
module Save
  CHECKPOINT_KEY = :checkpoint
  # What an object checkpoint says of an object without a physics body.
  NO_BODY = "none".freeze

  # Keeps this scene's spawned objects that persist something under `key`; returns how many.
  def self.checkpoint(key = CHECKPOINT_KEY)
    objects = Spawns.capture
    set(key, { "scene" => Game.scene_name, "objects" => objects })
    objects.size
  end

  # Spawns again what the checkpoint under `key` kept, in place of the spawned objects that persist
  # something now; returns the new copies ([] when nothing is kept there). A checkpoint of another
  # scene, or one naming a template or class the game no longer has, raises an ArgumentError and
  # changes nothing.
  def self.restore_checkpoint(key = CHECKPOINT_KEY)
    saved = get(key)
    saved ? Spawns.restore(saved) : []
  end

  # How a checkpoint is made and restored (the engine's own).
  module Spawns
    def self.capture = kept_objects.map { |object| record(object) }

    # Copies at the top of a spawn (not a part of a bigger copy) with a behaviour that persists.
    def self.kept_objects = Scene.objects.select { |object| kept?(object) }

    def self.kept?(object) = spawn_root?(object) && persists_any?(object)

    def self.spawn_root?(object) = object.stable_id.nil? && under_scene_object?(object)

    def self.under_scene_object?(object)
      parent = object.parent
      parent.nil? || !parent.stable_id.nil?
    end

    def self.persists_any?(object) = object.behaviours.any? { |behaviour| persists?(behaviour) }

    def self.persists?(behaviour) = behaviour.class.persisted_fields.any?

    def self.record(object)
      { "name" => object.name, "state" => object.capture_state,
        "behaviours" => object.behaviours.select { |behaviour| persists?(behaviour) }.map { |each| fields_of(each) } }
    end

    def self.fields_of(behaviour)
      fields = behaviour.class.persisted_fields.to_h { |field| [field.to_s, field_value(behaviour, field)] }
      { "class" => class_name(behaviour.class), "fields" => fields }
    end

    def self.class_name(klass)
      klass.name || raise(ArgumentError, "Save.checkpoint: a class with no name cannot persist fields")
    end

    def self.field_value(behaviour, field) = behaviour.instance_variable_get(:"@#{field}")

    # (Every template and class is found before anything is spawned or destroyed.)
    def self.restore(saved)
      require_scene(saved["scene"])
      plans = saved["objects"].map { |entry| plan(entry) }
      previous = kept_objects
      made = spawn_all(plans)
      previous.each(&:destroy)
      made
    end

    def self.require_scene(scene)
      return if scene == Game.scene_name

      raise ArgumentError, "the checkpoint was made in scene #{scene.inspect}, not in #{Game.scene_name.inspect}"
    end

    def self.plan(entry)
      state = entry["state"]
      { template: found(state["source"]), parent: parent_of(state), name: entry["name"], state: state,
        behaviours: entry["behaviours"].map { |each| [behaviour_class(each["class"]), each["fields"]] } }
    end

    def self.found(id)
      Scene.find_by_id(id) || raise(ArgumentError, "the checkpoint's object #{id.inspect} is not in this scene")
    end

    def self.parent_of(state) = state["parent"].empty? ? nil : found(state["parent"])

    def self.behaviour_class(name)
      klass = Object.const_get(name)
      raise ArgumentError, "the checkpoint's #{name} is not a Behaviour" unless behaviour_class?(klass)

      klass
    rescue NameError
      raise ArgumentError, "the checkpoint's behaviour #{name} is not in the game"
    end

    def self.behaviour_class?(klass) = klass.is_a?(Class) && klass <= Behaviour

    # (A copy that cannot take its place again goes, with those made before it.)
    def self.spawn_all(plans)
      made = []
      plans.each { |plan| fill(plan, made.push(spawned(plan)).last) }
      made
    rescue StandardError
      made.each(&:destroy)
      raise
    end

    # (A copy spawned without physics has no body in its checkpoint, and gets none again.)
    def self.spawned(plan)
      physics = plan[:state]["body"] != NO_BODY
      Scene.spawn(plan[:template], name: plan[:name], parent: plan[:parent], physics: physics) ||
        raise(ArgumentError, "the checkpoint's #{plan[:name]} could not be spawned again")
    end

    def self.fill(plan, copy)
      copy.restore_state(plan[:state])
      used = []
      plan[:behaviours].each { |klass, fields| assign(behaviour_for(copy, klass, used), fields) }
    end

    # The copy's next behaviour of that class (one its template gave it), else a new one.
    def self.behaviour_for(copy, klass, used)
      behaviour = copy.behaviours.find { |each| unused_of?(each, klass, used) }
      behaviour ||= copy.add_component(klass)
      used << behaviour
      behaviour
    end

    def self.unused_of?(behaviour, klass, used) = behaviour.instance_of?(klass) && !used.include?(behaviour)

    # (Only the fields the class still persists: one it stopped persisting is left as it starts.)
    def self.assign(behaviour, fields)
      wanted = behaviour.class.persisted_fields
      fields.each do |field, value|
        behaviour.instance_variable_set(:"@#{field}", value) if wanted.include?(field.to_sym)
      end
    end
  end
end
