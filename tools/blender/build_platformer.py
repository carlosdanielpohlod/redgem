"""Builds the platformer demo level: games/platformer/platformer.blend + scene.glb.

    tools/blender/run.sh build_platformer.py games/platformer

Open platformer.blend in Blender to edit the level, then re-export with
tools/blender/export_scene.py. Gameplay lives in custom properties (Object > Custom Properties).
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

GRASS = (0.06, 0.28, 0.04)
DIRT = (0.3, 0.14, 0.05)
PLATFORM = (0.04, 0.15, 0.6)
MOVER = (0.7, 0.12, 0.03)
GOLD = (1.0, 0.72, 0.05)
HERO = (0.9, 0.25, 0.02)
VISOR_COLOR = (0.1, 0.1, 0.15)
WOOD = (0.25, 0.1, 0.03)
LEAVES = (0.03, 0.22, 0.05)
FLAG_COLOR = (0.9, 0.1, 0.15)
POLE = (0.85, 0.85, 0.85)
WARM_LIGHT = (1.0, 0.8, 0.5)

QUARTER_TURN = math.radians(90.0)
GOLD_METALLIC = 0.8
GOLD_ROUGHNESS = 0.3

HERO_RADIUS = 0.35
HERO_HEIGHT = 1.6
HERO_SPAWN = (0.0, 0.0, 1.1)
COIN_RADIUS = 0.35
COIN_THICKNESS = 0.08
TRUNK_RADIUS = 0.25
TRUNK_HEIGHT = 2.0
CANOPY_RADIUS = 1.1
# How far the canopy center sits above the trunk top, as a fraction of its radius.
CANOPY_LIFT = 0.6
GOAL_TRIGGER_SCALE = (1.5, 1.5, 1.5)
CRATE_SIZE = 1.0
CRATE_MASS = 3.0

COIN_POSITIONS = [
    (0.0, 4.0, 1.0),
    (-4.0, 8.0, 1.0),
    (0.0, 13.0, 2.0),
    (0.0, 25.0, 2.6),
    (2.0, 31.0, 2.8),
]
TREE_POSITIONS = [(-5.5, 1.0), (5.5, 7.0), (-5.0, 10.0)]


# --- Level layout (Blender coordinates: x right, y forward, z up) ---------------------
GROUND = ((0.0, 4.0, -0.5), (14.0, 14.0, 1.0))  # (location, size); top face at z = 0
# Static steps, each (name, location, size).
STEPS = [
    ("Step1", (0.0, 13.0, 0.5), (4.0, 4.0, 1.0)),
    ("Step2", (0.0, 19.0, 1.25), (4.0, 4.0, 1.0)),
    ("GoalPad", (0.0, 31.0, 1.25), (5.0, 5.0, 1.0)),
]
MOVING_PLATFORM = ((0.0, 25.0, 1.5), (3.0, 3.0, 0.5))
MOVING_PLATFORM_PROPS = {"axis": "x", "distance": 2.5, "speed": 1.2}
HERO_PROPS = {"speed": 6.0, "jump_speed": 9.0}
VISOR = ((0.0, HERO_RADIUS, 0.45), (0.5, 0.12, 0.2))  # local to the hero
CRATE_STACK_XY = (4.0, 3.0)
CRATE_STACK_HEIGHT = 2
GOAL_TRIGGER_LOCATION = (0.0, 32.0, 2.75)
FLAG_POLE_LOCATION = (0.0, 32.5, 3.25)
FLAG_POLE_RADIUS = 0.08
FLAG_POLE_HEIGHT = 3.0
FLAG = ((0.6, 0.0, 1.1), (1.2, 0.05, 0.7))  # local to the pole
CAMERA_LOCATION = (0.0, -9.0, 6.0)
CAMERA_PROPS = {"target": "Player", "height": 5.0, "distance": 9.0}
SUN_LOCATION = (5.0, -5.0, 12.0)
SUN_TARGET = (0.0, 6.0, 0.0)
GOAL_LIGHT_LOCATION = (0.0, 30.0, 5.0)
ORIGIN = (0.0, 0.0, 0.0)


def build_ground():
    location, size = GROUND
    kit.box("Ground", location, size, mat=kit.material("Grass", GRASS), props={"collider": "box"})


def build_platforms():
    stone = kit.material("Platform", PLATFORM)
    for name, location, size in STEPS:
        kit.box(name, location, size, mat=stone, props={"collider": "box"})
    location, size = MOVING_PLATFORM
    kit.box("MovingPlatform", location, size, mat=kit.material("Mover", MOVER),
            props={"body": "kinematic", "script": "mover.rb", **MOVING_PLATFORM_PROPS})


def build_hero():
    hero = kit.cylinder("Player", HERO_SPAWN, HERO_RADIUS, HERO_HEIGHT, mat=kit.material("Hero", HERO),
                        props={"body": "character", "script": "player.rb", "tag": "player", **HERO_PROPS})
    # Visor on the hero's face (Blender +Y is the engine's forward).
    location, size = VISOR
    kit.box("Visor", location, size, mat=kit.material("Visor", VISOR_COLOR), parent=hero)


def build_coins():
    gold = kit.material("Gold", GOLD, roughness=GOLD_ROUGHNESS, metallic=GOLD_METALLIC)
    for index, position in enumerate(COIN_POSITIONS, start=1):
        kit.cylinder(f"Coin{index}", position, COIN_RADIUS, COIN_THICKNESS, mat=gold,
                     rotation=(QUARTER_TURN, 0.0, 0.0),
                     props={"trigger": True, "script": "coin.rb", "tag": "coin"})


def build_props():
    wood = kit.material("Wood", WOOD)
    leaves = kit.material("Leaves", LEAVES)
    for index, (x, y) in enumerate(TREE_POSITIONS, start=1):
        trunk = kit.cylinder(f"Tree{index}", (x, y, TRUNK_HEIGHT / 2), TRUNK_RADIUS, TRUNK_HEIGHT, mat=wood,
                             props={"collider": "capsule"})
        kit.sphere(f"Canopy{index}", (0.0, 0.0, TRUNK_HEIGHT / 2 + CANOPY_RADIUS * CANOPY_LIFT), CANOPY_RADIUS,
                   mat=leaves, parent=trunk)
    crate = kit.material("Crate", DIRT)
    x, y = CRATE_STACK_XY
    for level in range(CRATE_STACK_HEIGHT):
        z = CRATE_SIZE / 2 + level * CRATE_SIZE
        kit.box(f"Crate{level + 1}", (x, y, z), (CRATE_SIZE, CRATE_SIZE, CRATE_SIZE), mat=crate,
                props={"body": "dynamic", "mass": CRATE_MASS, "tag": "crate"})


def build_goal():
    kit.empty("Goal", GOAL_TRIGGER_LOCATION, props={"trigger": True, "script": "goal.rb"}).scale = GOAL_TRIGGER_SCALE
    pole = kit.cylinder("FlagPole", FLAG_POLE_LOCATION, FLAG_POLE_RADIUS, FLAG_POLE_HEIGHT,
                        mat=kit.material("Pole", POLE))
    location, size = FLAG
    kit.box("Flag", location, size, mat=kit.material("Flag", FLAG_COLOR), parent=pole)


def build_world():
    kit.empty("GameManager", ORIGIN, props={"script": "game_manager.rb"})
    kit.camera("Camera", CAMERA_LOCATION, HERO_SPAWN, props={"script": "camera_follow.rb", **CAMERA_PROPS})
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET)
    kit.point_light("GoalLight", GOAL_LIGHT_LOCATION, WARM_LIGHT)


def main():
    out_dir = kit.output_path_from_argv("games/platformer")
    kit.reset_scene()
    build_ground()
    build_platforms()
    build_hero()
    build_coins()
    build_props()
    build_goal()
    build_world()
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "platformer.blend"))
    kit.export_glb(os.path.join(out_dir, "scene.glb"))


main()
