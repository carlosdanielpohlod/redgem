# Network.spawn: objects made on every machine in the game, each moved by the machine that owns it.
module Network
  @spawned = {}      # key => { template:, owner:, object: }
  @spawnable = []    # the templates other machines may make this one spawn
  @spawn_count = 0   # what this machine spawned so far: the keys it gives end with it

  class << self
    # Copies a template (see Scene.spawn) on every machine in the game, now and for whoever joins
    # later. The copy belongs to `owner`: that machine moves it, with its physics, and the others
    # watch it move (it gets a NetworkSync, and no physics there). It goes away with its owner.
    # A machine has only so many at a time, and so does a game (MAX_SPAWNS_PER_PEER, MAX_SPAWNS):
    # past that nothing is made, here or anywhere, and nil comes back.
    def spawn(template, owner: id, position: nil, rotation: nil)
      return refuse_spawn(template, owner) unless room_for?(owner)

      allow_spawn(template)
      record = [next_key(template), template.to_s, owner, position&.to_a, rotation&.to_a]
      __deliver(SPAWN, record, :all)
      place(record)
    end

    # Other machines may only make this one spawn templates it spawns itself, `prefab = true`
    # objects, and the ones named here.
    def allow_spawn(*templates)
      templates.each { |template| @spawnable << template.to_s unless @spawnable.include?(template.to_s) }
    end

    private

    def refuse_spawn(template, owner)
      Debug.warn("Network.spawn(#{template}): machine #{owner} or the game has all the spawned objects it may")
      nil
    end

    def enter_game(_peer)
      @my_id = id
      adopt_local_objects
    end

    def next_key(template)
      @spawn_count += 1
      "#{template}##{id}.#{@spawn_count}"
    end

    # What `owner` has spawned: [key, entry] pairs.
    def spawned_by(owner) = @spawned.to_a.select { |_key, entry| entry[:owner] == owner }

    def keys_spawned_by(owner) = spawned_by(owner).map(&:first)

    def place(record)
      key, template, owner, position, rotation = record
      return @spawned[key][:object] if @spawned[key]

      mine = !online? || owner == id
      object = Scene.spawn(template, position: position && Vector3[*position],
                                     rotation: rotation && Vector3[*rotation], physics: mine)
      object.add_component(NetworkSync, owner: owner, key: key)
      @spawned[key] = { template: template, owner: owner, object: object }
      object
    end

    def record_of(key, entry)
      transform = entry[:object].transform
      [key, entry[:template], entry[:owner], transform.position.to_a, transform.rotation.to_a]
    end

    # This machine just got into a game: what it spawned while in none becomes its own there, under
    # names nobody else uses, and the others are told.
    def adopt_local_objects
      spawned_by(LOCAL).each { |key, entry| adopt(key, entry) if entry[:object].alive? }
    end

    def adopt(local_key, entry)
      @spawned.delete(local_key)
      entry[:owner] = id
      key = next_key(entry[:template])
      @spawned[key] = entry
      entry[:object].get_component(NetworkSync).assign(owner: id, key: key)
      __deliver(SPAWN, record_of(key, entry), :all)
    end

    def remove_objects_of(peer)
      keys_spawned_by(peer).each { |key| remove(key) }
    end

    def remove_objects(keys, owner)
      keys.each { |key| remove(key) if owns?(owner, key) }
    end

    def owns?(owner, key) = @spawned[key] && @spawned[key][:owner] == owner

    def remove(key)
      @spawned.delete(key)[:object].destroy
    end

    # The game is gone: what this machine was moving stays, as its own; the rest leaves with the
    # others.
    def keep_only_my_objects(_peer)
      @spawned.to_a.each do |key, entry|
        sync = entry[:object].get_component(NetworkSync)
        next remove(key) unless sync.owner == @my_id

        entry[:owner] = LOCAL
        sync.owner = LOCAL
      end
    end

    def announce_scene_reset
      mine = keys_spawned_by(id)
      __deliver(DESPAWN, mine, :all) unless mine.empty?
      __deliver(RESEND, nil, :host) unless host?
    end
  end
end
