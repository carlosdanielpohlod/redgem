# The save: the level the party is on and its quests (see Quests.save), kept with Save so that
# they last across levels and between runs. Only the machine that decides writes it: the one
# playing alone, the host, or the dedicated server (in the server's own database). The other
# players hear of the quests from it and keep nothing.
module Progress
  LEVEL = :level
  QUESTS = :quests

  # Whether this machine decides the party's progress.
  def self.decides? = !Network.online? || Network.host?

  # Called by every level's setup: saves on every change of a quest, and saves the level each
  # time one loads (but the first, which only shows the title). A dedicated server goes on from
  # its own save as it starts.
  def self.follow(first_level:)
    Quests.on(:change) { |_quest| save }
    save unless first_level
    Quests.restore(QUESTS) if first_level && Network.server?
  end

  def self.save
    return unless decides?

    Save.set(LEVEL, Game.scene_name)
    Quests.save(QUESTS)
  end

  # The level saved, or nil.
  def self.level = Save.get(LEVEL)

  # Puts the saved quests back and goes to the saved level.
  def self.resume
    Quests.restore(QUESTS)
    saved = level
    Game.load_scene(saved) if saved && saved != Game.scene_name
  end

  # Starts over: nothing saved.
  def self.clear = [LEVEL, QUESTS].each { |key| Save.delete(key) }
end
