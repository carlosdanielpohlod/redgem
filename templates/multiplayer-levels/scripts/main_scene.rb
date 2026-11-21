# Sets every level up when it loads: this one (scene.glb, "main") and the second one
# (scenes/level2.glb, see Level2Scene). Each part of the game is a script of its own:
#   Player      the hero, walked with the keys or a gamepad
#   Spawns      where each player's hero starts a level
#   Party       the other players' heroes, in a game played together
#   Session     playing alone, hosting, joining, or joining a dedicated server
#   LobbyScreen the menu's screen for that
#   LevelExit   the doors that take the whole party to another level
#   Progress    the save: the level the party is on and its quest, kept across levels and runs
#   Guide       someone to talk to (dialogues/guide.json), who gives the quest (quests/beacon.json)
#   Hud         what the player reads on screen
# A dedicated server runs these scripts too, with no hero, menu or HUD of its own.
class MainScene < SceneScript
  HERO = "Player".freeze

  # Whether a level was set up already in this run: the title screen opens on the first one only.
  def self.started? = @started == true

  def self.started! = @started = true

  def setup
    first = !MainScene.started?
    MainScene.started!
    Camera.main.add_component(Party)
    Progress.follow(first_level: first)
    return find!(HERO).destroy if Network.server?

    hero_parts(find!(HERO))
    LobbyScreen.install(Camera.main.add_component(GameMenu, start_open: first))
  end

  private

  # What lets the hero use doors, talk, read a conversation and follow the quest.
  def hero_parts(hero)
    hero.add_component(Interactor)
    hero.add_component(DialogueBox)
    hero.add_component(QuestTracker)
    Camera.main.add_component(Hud)
  end
end
