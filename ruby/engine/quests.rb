# The game's quests, written in quest files: quests/<name>.json, each a set of stages, and in each
# stage what the party does (its objectives) before the quest goes on to the next one:
#
#   {
#     "title": "bread.title",
#     "start": "flour",
#     "stages": {
#       "flour": { "text": "bread.flour", "objectives": [
#                    { "go_to": "Mill", "radius": 3, "text": "bread.go_to_mill" },
#                    { "collect": "flour", "count": 3, "text": "bread.collect" } ], "next": "bring" },
#       "bring": { "text": "bread.bring", "objectives": [
#                    { "deliver": "flour", "to": "Baker", "count": 3, "text": "bread.deliver" },
#                    { "talk_to": "Baker", "dialogue": "baker", "node": "thanks", "text": "bread.talk" } ],
#                  "next": "done" },
#       "done": { "text": "bread.done", "end": true }
#     }
#   }
#
# Texts are I18n keys (an objective's may show {progress} and {count}). An objective is one of
# go_to (a place, an object's name: the QuestTracker of a hero that comes within "radius" of it),
# talk_to (someone: a conversation with them as the speaker, of that "dialogue" and at that "node"
# when they are given), deliver and collect (an item, "count" of them: the game says so with
# Quests.delivered and Quests.collected), defeat (someone: Quests.defeated) and check (a check of the
# game's own, Quests.check). Once every objective of a stage is done the quest goes on to its
# "next"; a stage with "end": true completes it, and one with neither waits for a dialogue's effect
# or a script (Quests.set_stage) to move it on.
#
#   Quests.start(:bread)
#   Quests.collected(:flour)                          # a sack was picked up
#   Quests.check(:oven_hot) { |quest| GameVariables[:oven] == "hot" }
#   Quests.on(:complete) { |quest| GameVariables[:gold] = GameVariables[:gold].to_i + 10 }
#   Quests[:bread].objectives.map(&:text)             # => ["Go to the mill", "Collect flour (1/3)"]
#
# A quest's state is kept in GameVariables ("quest.<name>" its stage, "quest.<name>#<n>" how far
# objective n is), so the whole party shares it and the host owns it: on a client, Quests.start and
# set_stage raise, and what a client's player collects, delivers, defeats or reaches is told to the
# host, which counts it. A game saves the quests with Quests.save (Save.set(:quests, Quests.to_h))
# and puts them back with Quests.restore. What Quests.on and Quests.check register lasts until the
# next level is loaded. `redgem check` reads every quest file (see docs/scripting/quests.md).
module Quests
  PREFIX = "quest.".freeze
  PROGRESS_MARK = "#".freeze
  EVENTS = %i[start stage objective complete change].freeze
  ITEM_KINDS = %i[deliver collect].freeze
  REPORTED = %w[go_to deliver collect defeat].freeze   # what a client tells the host its player did
  MAX_STEPS = 64          # stages one change may go through: past this, the stages go round
  MAX_COUNT = 1_000_000   # items, foes or places one report may count
  SAVE_KEY = :quests
  @handlers = {}
  @checks = {}
  @definitions = {}

  class << self
    # The quest of that name (a Quest), started or not. Raises for one with no file in quests/.
    def [](name) = Quest.new(name.to_s, load(name))

    # The quest file of that name, read once. Raises for one that is not there or does not read.
    def load(name) = definition_of(name) || Definition.new(name.to_s, __read(name.to_s))

    # Every quest that started (completed ones too), and those that are under way or completed.
    def started = started_names.map { |name| quest_named(name) }
    def active = started.select(&:active?)
    def completed = started.select(&:complete?)

    # The stage a quest is at, or nil while it has not started.
    def stage(name) = GameVariables[PREFIX + name.to_s]

    # Starts a quest at its first stage. False when it had started already.
    def start(name)
      return false if stage(name)

      set_stage(name, load(name).start)
      true
    end

    # Moves a quest to a stage (nil: as if it never started); its objectives start from nothing.
    # Raises on a client, and for a stage its file does not have. A quest with no file is only its
    # stage (what dialogues of a game with no quests folder move). The stage.
    def set_stage(name, stage, _conversation = nil)
      host_only!("move a quest")
      name = name.to_s
      return GameVariables[PREFIX + name] = stage unless definition_of(name)

      move(name, checked_stage(name, stage))
      stage
    end

    def reset(name) = set_stage(name, nil)

    # What the players did, for the objectives that ask for it (on a client, told to the host).
    def collected(item, count = 1) = advance("collect", item.to_s, nil, count)
    def delivered(item, to:, count: 1) = advance("deliver", item.to_s, name_of(to), count)
    def defeated(who, count = 1) = advance("defeat", name_of(who), nil, count)
    def reached(place) = advance("go_to", name_of(place), nil, 1)

    # A check of the game's own, for the objectives { "check": name }: |quest| true once it is passed.
    # It is asked every frame (by a QuestTracker) on the host, while such an objective waits.
    def check(name, &block)
      raise ArgumentError, "Quests.check needs a block saying whether #{name} is passed" unless block

      @checks[name.to_s] = block
    end

    # :start, :complete |quest|; :stage |quest, stage, from|; :objective |quest, objective| (it moved
    # on); :change |quest| (anything of it changed, also as quests are put back or a newcomer hears
    # of them, which tells nothing else). On every machine, as its quests change.
    def on(event, &block)
      raise ArgumentError, "Quests.on takes #{EVENTS.inspect}, not #{event.inspect}" unless EVENTS.include?(event)

      (@handlers[event] ||= []) << block
    end

    # The quests that started, for Save: { "bread" => { "stage" => "bring", "progress" => [2, 0] } }.
    def to_h = started_names.to_h { |name| [name, { "stage" => stage(name), "progress" => progress_of(name) }] }

    # Puts these in the place of every quest's state (what to_h gave, maybe migrated by Save). A
    # quest whose file no longer has the stage is left out, said in the log. Raises on a client.
    def replace(saved)
      host_only!("put quests back")
      values = GameVariables.to_h.reject { |key, _value| key.start_with?(PREFIX) }
      (saved || {}).to_a.each { |name, state| values.merge!(variables_of(name.to_s, state)) }
      GameVariables.replace(values)
      to_h
    end

    # The quest this machine's journal and markers follow (see GameMenu::JournalScreen and
    # QuestMarkers): the one tracked while it is under way, else the first one under way; nil when
    # none is. Each machine tracks its own, though the party shares the quests.
    def tracked = active.find { |quest| quest.name == @tracked } || active.first

    # Tracks a quest by its name (nil: the first one under way).
    def track(name) = @tracked = name&.to_s

    # Whether the game has quest files at all (its menu then has a journal).
    def any? = !__names.empty?

    def save(key = SAVE_KEY) = Save.set(key, to_h)
    def restore(key = SAVE_KEY) = replace(Save.get(key, {}))

    # How far an objective of the stage a quest is at has come.
    def __progress(name, index) = GameVariables[progress_key(name, index)] || 0

    # Called by GameVariables as one changes (here, or from the host). bulk: as all were put back.
    def __variable(name, value, was, bulk)
      return unless name.start_with?(PREFIX)

      quest_name, index = name[PREFIX.size..].split(PROGRESS_MARK, 2)
      quest = quest_named(quest_name)
      return tell(:change, quest) if bulk

      index ? progressed(quest, index.to_i, value) : moved(quest, value, was)
    end

    # Called by a conversation that decides as it reaches a node (on the host, or offline).
    def __talked(conversation)
      matching { |objective| talked_to?(objective, conversation) }.each { |objective| add(objective, 1) }
      settle_all
    end

    # What a player did, counted (here, or on the host for a client: see network_quests.rb).
    def __report(kind, subject, extra, count)
      matching { |objective| did?(objective, kind, subject, extra) }.each { |objective| add(objective, count) }
      settle_all
    end

    # The host (or this machine, offline) asks the game's checks, and moves quests on.
    def __poll
      return if client?

      active.each { |quest| poll(quest.name) }
    end

    # Called by the engine when a level is loaded.
    def __scene_reset
      @handlers = {}
      @checks = {}
      @definitions = {}
    end

    private

    def client? = Network.online? && !Network.host?

    def host_only!(what)
      raise ArgumentError, "only the host can #{what} in a game over the network" if client?
    end

    # (A quest with no file is remembered as such: false.)
    def definition_of(name)
      name = name.to_s
      @definitions[name] = __exists?(name) && Definition.new(name, __read(name)) unless @definitions.key?(name)
      @definitions[name] || nil
    end

    def quest_named(name) = Quest.new(name, definition_of(name))

    def started_names = GameVariables.to_h.keys.select { |key| stage_key?(key) }.map { |key| key[PREFIX.size..] }

    def stage_key?(key) = key.start_with?(PREFIX) && !key.include?(PROGRESS_MARK)

    def progress_key(name, index) = "#{PREFIX}#{name}#{PROGRESS_MARK}#{index}"

    def progress_of(name) = quest_named(name).objectives.map(&:progress)

    def name_of(object) = object.respond_to?(:name) ? object.name.to_s : object.to_s

    def checked_stage(name, stage)
      return stage if stage.nil? || definition_of(name).stage?(stage)

      stages = definition_of(name).stage_ids.join(", ")
      raise ArgumentError, "quest #{name} has no stage #{stage.inspect} (it has #{stages})"
    end

    def tell(event, *values) = (@handlers[event] || []).dup.each { |block| block.call(*values) }
  end
end
