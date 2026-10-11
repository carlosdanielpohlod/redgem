"""Builds the driving demo: games/driving/driving.blend + scene.glb.

    tools/blender/run.sh build_driving.py games/driving

The car is a box chassis with `body = "vehicle"`; its four wheel children carry `wheel = true`
and are modeled with their axle along X.
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

GRASS = (0.08, 0.3, 0.05)
ASPHALT = (0.03, 0.03, 0.035)
CAR_PAINT = (0.6, 0.02, 0.02)
GLASS = (0.05, 0.08, 0.12)
TIRE = (0.02, 0.02, 0.02)
CONE = (0.9, 0.25, 0.01)
RAMP = (0.5, 0.45, 0.1)
GATE = (0.95, 0.95, 0.95)
BANNER = (0.05, 0.3, 0.9)
LIGHT = (1.0, 0.95, 0.6)

HALF = 0.5                      # centers and half-extents are half a size
Z = 2                           # index of the height in an (x, y, z) size
# How matte each surface is (0 = mirror, 1 = chalk).
ASPHALT_ROUGHNESS = 0.9
PAINT_ROUGHNESS = 0.3
GLASS_ROUGHNESS = 0.1
TIRE_ROUGHNESS = 0.95

QUARTER_TURN = math.radians(90.0)
RAMP_ANGLE = math.radians(12.0)

GROUND_SIZE = 260.0
ROAD_WIDTH = 10.0
ROAD_THICKNESS = 0.02
# Road loop corners (Blender x, y): start straight north, then east, south and back west.
LOOP_MIN_X, LOOP_MAX_X = 0.0, 60.0
LOOP_MIN_Y, LOOP_MAX_Y = -15.0, 60.0

CAR_START = (0.0, 0.0, 1.0)
CHASSIS_SIZE = (1.8, 4.0, 0.6)
CABIN_SIZE = (1.5, 2.0, 0.5)
WHEEL_RADIUS = 0.4
WHEEL_WIDTH = 0.3
WHEEL_X = 0.95
WHEEL_Y = 1.3
WHEEL_Z = -0.35
# (name, x, y) of each wheel, relative to the chassis center.
WHEELS = (("WheelFL", -WHEEL_X, WHEEL_Y), ("WheelFR", WHEEL_X, WHEEL_Y),
          ("WheelRL", -WHEEL_X, -WHEEL_Y), ("WheelRR", WHEEL_X, -WHEEL_Y))

CAR_MASS = 1200.0
CABIN_OFFSET_Y = -0.3
GROUND_CENTER = (0.0, 20.0, -0.5)
GROUND_THICKNESS = 1.0
RAMP_LOCATION = (LOOP_MIN_X, 45.0, 0.3)
RAMP_SIZE = (6.0, 8.0, 0.6)

CONE_SIZE = (0.5, 0.5, 0.8)
CONE_MASS = 4.0
CONE_SPOTS = [(4.0, 10.0), (-4.0, 18.0), (4.0, 26.0), (25.0, 56.0), (35.0, 64.0)]

GATE_POST_RADIUS = 0.2
GATE_HEIGHT = 4.0
GATE_DEPTH = 1.0
GATE_MARGIN = 2.0
BANNER_THICKNESS = 0.3
BANNER_HEIGHT = 0.8
GATE_WIDTH = ROAD_WIDTH + GATE_MARGIN
GATE_SIDES = (("L", -1.0), ("R", 1.0))   # (post name suffix, which end of the gate)
ORIGIN = (0.0, 0.0, 0.0)
CAMERA_LOCATION = (0.0, -10.0, 5.0)
CAMERA_PROPS = {"target": "Car", "distance": 10.0, "height": 4.0}
SUN_LOCATION = (20.0, -20.0, 30.0)
SUN_TARGET = (30.0, 25.0, 0.0)
START_LIGHT_LOCATION = (0.0, 0.0, 6.0)
# (location, gate spans the X axis?) in driving order around the loop.
CHECKPOINTS = [
    ((LOOP_MIN_X, 30.0, 0.0), True),
    ((30.0, LOOP_MAX_Y, 0.0), False),
    ((LOOP_MAX_X, 20.0, 0.0), True),
    ((30.0, LOOP_MIN_Y, 0.0), False),
]


def build_ground_and_roads():
    kit.box("Ground", GROUND_CENTER, (GROUND_SIZE, GROUND_SIZE, GROUND_THICKNESS), mat=kit.material("Grass", GRASS),
            props={"collider": "box"})
    asphalt = kit.material("Asphalt", ASPHALT, roughness=ASPHALT_ROUGHNESS)
    length_y = LOOP_MAX_Y - LOOP_MIN_Y + ROAD_WIDTH
    length_x = LOOP_MAX_X - LOOP_MIN_X + ROAD_WIDTH
    center_y = (LOOP_MIN_Y + LOOP_MAX_Y) * HALF
    center_x = (LOOP_MIN_X + LOOP_MAX_X) * HALF
    road_z = ROAD_THICKNESS * HALF
    # Visual only: the ground collider carries the car.
    kit.box("RoadWest", (LOOP_MIN_X, center_y, road_z), (ROAD_WIDTH, length_y, ROAD_THICKNESS), mat=asphalt)
    kit.box("RoadEast", (LOOP_MAX_X, center_y, road_z), (ROAD_WIDTH, length_y, ROAD_THICKNESS), mat=asphalt)
    kit.box("RoadNorth", (center_x, LOOP_MAX_Y, road_z), (length_x, ROAD_WIDTH, ROAD_THICKNESS), mat=asphalt)
    kit.box("RoadSouth", (center_x, LOOP_MIN_Y, road_z), (length_x, ROAD_WIDTH, ROAD_THICKNESS), mat=asphalt)


def build_car():
    car = kit.box("Car", CAR_START, CHASSIS_SIZE, mat=kit.material("Paint", CAR_PAINT, roughness=PAINT_ROUGHNESS),
                  props={"body": "vehicle", "mass": CAR_MASS, "script": "car.rb", "tag": "player"}, bake_scale=True)
    kit.box("Cabin", (0.0, CABIN_OFFSET_Y, (CHASSIS_SIZE[Z] + CABIN_SIZE[Z]) * HALF), CABIN_SIZE,
            mat=kit.material("Glass", GLASS, roughness=GLASS_ROUGHNESS), parent=car, bake_scale=True)
    tire = kit.material("Tire", TIRE, roughness=TIRE_ROUGHNESS)
    for name, x, y in WHEELS:
        kit.cylinder(name, (x, y, WHEEL_Z), WHEEL_RADIUS, WHEEL_WIDTH, mat=tire, parent=car,
                     rotation=(0.0, QUARTER_TURN, 0.0), props={"wheel": True})


def build_obstacles():
    ramp = kit.box("Ramp", RAMP_LOCATION, RAMP_SIZE, mat=kit.material("Ramp", RAMP), props={"collider": "box"})
    ramp.rotation_euler = (RAMP_ANGLE, 0.0, 0.0)
    cone = kit.material("Cone", CONE)
    for index, (x, y) in enumerate(CONE_SPOTS, start=1):
        kit.box(f"Cone{index}", (x, y, CONE_SIZE[Z] * HALF), CONE_SIZE, mat=cone,
                props={"body": "dynamic", "mass": CONE_MASS, "tag": "cone"})


def along_gate(across_x, length, thickness, height):
    """Size of a box lying along a gate, which spans the X axis or the Y axis."""
    return (length, thickness, height) if across_x else (thickness, length, height)


def build_gate_posts(order, x, y, across_x):
    white = kit.material("Gate", GATE)
    for side_name, side in GATE_SIDES:
        offset = side * GATE_WIDTH * HALF
        post = (x + offset, y, GATE_HEIGHT * HALF) if across_x else (x, y + offset, GATE_HEIGHT * HALF)
        kit.cylinder(f"Checkpoint{order}Post{side_name}", post, GATE_POST_RADIUS, GATE_HEIGHT, mat=white)


def build_checkpoint(order, location, across_x):
    """Invisible trigger gate plus its decoration (posts and a banner the race manager toggles)."""
    x, y, _ = location
    trigger = kit.empty(f"Checkpoint{order}", (x, y, GATE_HEIGHT * HALF),
                        props={"trigger": True, "script": "checkpoint.rb", "order": order, "tag": "checkpoint"})
    trigger.scale = along_gate(across_x, GATE_WIDTH, GATE_DEPTH, GATE_HEIGHT)
    build_gate_posts(order, x, y, across_x)
    kit.box(f"Checkpoint{order}Banner", (x, y, GATE_HEIGHT - BANNER_HEIGHT * HALF),
            along_gate(across_x, GATE_WIDTH, BANNER_THICKNESS, BANNER_HEIGHT), mat=kit.material("Banner", BANNER))


def build_checkpoints():
    for order, (location, across_x) in enumerate(CHECKPOINTS, start=1):
        build_checkpoint(order, location, across_x)


def build_world():
    kit.empty("RaceManager", ORIGIN, props={"script": "race_manager.rb"})
    kit.camera("Camera", CAMERA_LOCATION, CAR_START, props={"script": "chase_camera.rb", **CAMERA_PROPS})
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET)
    kit.point_light("StartLight", START_LIGHT_LOCATION, LIGHT)


def main():
    out_dir = kit.output_path_from_argv("games/driving")
    kit.reset_scene()
    build_ground_and_roads()
    build_car()
    build_obstacles()
    build_checkpoints()
    build_world()
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "driving.blend"))
    kit.export_glb(os.path.join(out_dir, "scene.glb"))


main()
