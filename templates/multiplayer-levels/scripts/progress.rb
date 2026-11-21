# The save: the level the party is on and its quests (see Quests.save), kept with Save so that
# they last across levels and between runs. A machine writes it only while its own save is in
# play (after Play, Host, or the dedicated server's start: Progress.resume) and it decides the
# party's progress (alone, the host, or the dedicated server, in the server's own database). A
# player who joins someone else's game plays the host's progress and keeps nothing of it.
module Progress
  LEVEL = :level
  QUESTS = :quests

  # Whether this machine keeps the party's progress now.
  def self.decides? = @own == true && (!Network.online? || Network.host?)

  # Called by every level's setup: saves the level, and every change of a quest from now on.
  def self.follow
    Quests.on(:change) { |_quest| save }
    save
  end

  def self.save
    return unless decides?

    Save.set(LEVEL, Game.scene_name)
    Quests.save(QUESTS)
  end

  # The level saved, or nil.
  def self.level = Save.get(LEVEL)

  # Puts this machine's own save in play: its quests come back and the party goes to its level.
  def self.resume
    saved = level   # (read first: putting the quests back saves the level that runs now)
    @own = true
    Quests.restore(QUESTS)
    Game.load_scene(saved) if saved && saved != Game.scene_name
  end

  # Another machine's game is being joined: what it plays is not this machine's to save.
  def self.release = @own = false

  # Starts over: nothing saved.
  def self.clear = [LEVEL, QUESTS].each { |key| Save.delete(key) }
end
