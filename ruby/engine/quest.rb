# What a quest is (Quests::Definition, its file as read) and where the party is in it (Quests::Quest
# and its Quests::Objective, which read the quest's state each time they are asked). See quests.rb.
module Quests
  # A quest file as it was read (string keys). The engine refuses one that is not well made, so
  # every stage it names is there and every objective is one it knows.
  class Definition
    attr_reader :name

    def initialize(name, data)
      @name = name
      @data = data
    end

    def title_key = @data["title"]
    def description_key = @data["description"]
    def start = @data["start"]
    def stage(id) = @data["stages"][id]
    def stage?(id) = @data["stages"].key?(id)
    def stage_ids = @data["stages"].keys

    # Whether reaching a stage completes the quest.
    def end?(id) = stage?(id) && stage(id)["end"] == true

    # The objectives of a stage (Hashes), in order.
    def objectives(id) = (stage?(id) && stage(id)["objectives"]) || []

    def inspect = "#<Quests::Definition #{name}>"
  end

  # One quest and where the party is in it. Quests[name] gives one.
  class Quest
    attr_reader :name, :definition

    def initialize(name, definition)
      @name = name
      @definition = definition
    end

    # The stage it is at (its id), or nil while it has not started.
    def stage = Quests.stage(name)

    def started? = !stage.nil?
    def complete? = !definition.nil? && definition.end?(stage)
    def active? = started? && !complete?

    def title = definition && I18n.t(definition.title_key)
    def description = definition&.description_key && I18n.t(definition.description_key)

    # The text of the stage it is at, or nil.
    def stage_text = stage_data && I18n.t(stage_data["text"])

    # What the party does now: the objectives of the stage it is at.
    def objectives
      return [] unless definition && stage

      definition.objectives(stage).each_index.map { |index| Objective.new(self, index) }
    end

    # The stage it goes to once its objectives are done, or nil.
    def next_stage = stage_data && stage_data["next"]

    def ==(other) = other.is_a?(Quest) && other.name == name
    def inspect = "#<Quests::Quest #{name} at #{stage.inspect}>"

    private

    def stage_data = definition && stage && definition.stage(stage)
  end

  # One thing the party does in a stage of a quest: go to a place, talk to someone, deliver or
  # collect items, defeat someone, or pass a check of the game's own.
  class Objective
    KINDS = %w[go_to talk_to deliver collect defeat check].freeze
    COUNTED = %w[deliver collect defeat].freeze
    RADIUS = 2.0   # meters from a go_to's place that reach it, unless the file says

    attr_reader :quest, :index

    def initialize(quest, index)
      @quest = quest
      @index = index
      @stage = quest.stage
    end

    # :go_to, :talk_to, :deliver, :collect, :defeat or :check.
    def kind = kind_name.to_sym

    # What it names: a place, someone, an item, or a check.
    def target = data[kind_name]

    # How many it takes (a deliver, collect or defeat's "count"; 1 for the others), and how many it has.
    def count = data["count"] || 1
    def progress = __current? ? Quests.__progress(quest.name, index) : 0
    def done? = progress >= count

    # Its text, with {progress} and {count} filled in.
    def text = I18n.t(data["text"], progress: progress, count: count)

    def radius = data["radius"] || RADIUS
    def recipient = data["to"]          # a deliver's: who the items go to
    def dialogue = data["dialogue"]     # a talk_to's: the dialogue it must be, or nil
    def node = data["node"]             # ... and the node it must reach, or nil

    # The objective's line in the file (a Hash).
    def data = quest.definition.objectives(@stage)[index]

    def __current? = quest.stage == @stage

    def inspect = "#<Quests::Objective #{quest.name} #{kind} #{target} #{progress}/#{count}>"

    private

    def kind_name = KINDS.find { |name| data.key?(name) }
  end
end
