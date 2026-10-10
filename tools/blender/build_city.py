"""Builds the city demo from CC0 Kenney packs: games/city/city.blend + scene.glb.

    tools/assets/fetch_kenney.sh                     # once: downloads the packs into .deps/assets
    tools/blender/run.sh build_city.py games/city

An avenue (along Blender Y) crosses a street (along X). Four blocks around the crossroad hold
buildings, tree verges and small parks. Moving cars carry traffic_car.rb; a TrafficController
switches the lights; the camera orbits the crossroad.

Kenney models are 1 unit per road tile; everything is laid out in tile units and scaled by TILE.
"""

import math
import os
import random
import sys

import bpy
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(REPO_ROOT, ".deps", "assets")
ROADS = os.path.join(ASSETS, "city-kit-roads", "Models", "GLB format")
CARS = os.path.join(ASSETS, "car-kit", "Models", "GLB format")
NATURE = os.path.join(ASSETS, "nature-kit", "Models", "GLTF format")
COMMERCIAL = os.path.join(ASSETS, "city-kit-commercial", "Models", "GLB format")

TILE = 8.0                 # meters per Kenney road tile
CAR_SCALE = 0.22 * TILE    # cars are modeled ~1.5 units wide; this fits two lanes per tile
TREE_SCALE = 5.0
BUSH_SCALE = 4.0
SEED = 7

QUARTER_TURN = 90.0
HALF_TURN = 180.0
AVENUE_HALF_LENGTH = 6     # tiles from the crossroad to each end of the avenue
STREET_HALF_LENGTH = 5
BLOCK_DEPTH = 4            # tiles of buildings behind each verge
LANE_OFFSET = 0.22         # tiles from the road center line to a lane center
CAR_HEIGHT = 0.02          # tiles: road surface thickness
VERGE = 1                  # first tile next to a road: trees and lamps
PARK_TILES = {(3, 3), (3, 4), (4, 3)}  # (x, y) block tiles left as parks in each block

GRASS = (0.12, 0.3, 0.06)
GROUND_THICKNESS = 0.5
GROUND_MARGIN = 2          # tiles of grass around the blocks

BUILDINGS = [f"building-{c}" for c in "abcdefghijklmn"] + [f"building-skyscraper-{c}" for c in "abcde"]
TREES = ["tree_oak", "tree_default", "tree_detailed", "tree_fat"]
BUSHES = ["plant_bush", "plant_bushLarge", "plant_bushDetailed"]

# Moving cars: (model, axis, direction along it, start position in tiles along the axis).
TRAFFIC = [
    ("sedan", "avenue", 1, -5.0), ("taxi", "avenue", 1, -2.5), ("police", "avenue", 1, 3.0),
    ("van", "avenue", -1, 4.5), ("suv", "avenue", -1, 1.5), ("hatchback-sports", "avenue", -1, -4.0),
    ("delivery", "street", 1, -4.0), ("sedan-sports", "street", -1, 3.5),
]
CAR_SPEED = 9.0            # m/s
# Parked cars: (model, x tile, y tile, heading degrees).
PARKED = [("sedan", 0.38, -3.0, 0.0), ("suv-luxury", 0.38, -4.2, 0.0), ("taxi", -0.38, 2.8, HALF_TURN),
          ("van", -3.0, 0.38, QUARTER_TURN)]

LAMP_SPACING = 2           # every other verge tile gets a lamp, the others a tree
SUN_LOCATION = (60.0, -80.0, 70.0)
SUN_TARGET = (0.0, 0.0, 0.0)
SUN_STRENGTH = 3.2
WARM_LIGHT = (1.0, 0.75, 0.45)
CORNER_LIGHT_HEIGHT = 6.0
CORNER_LIGHT_WATTS = 900.0
CAMERA_LOCATION = (60.0, -60.0, 45.0)
CAMERA_PROPS = {"radius": 85.0, "height": 45.0, "speed": 0.06, "look_height": 0.0}
TRAFFIC_CYCLE_SECONDS = 7.0


# --- Model library: import each GLB once, place linked copies (shared meshes) ---------------

_library = {}


def load(folder, model):
    """Imports a model once and returns its mesh parts as (object, matrix relative to its origin)."""
    key = os.path.join(folder, model)
    if key in _library:
        return _library[key]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(folder, model + ".glb"))
    created = [obj for obj in bpy.data.objects if obj not in before]
    parts = [(obj, obj.matrix_world.copy()) for obj in created if obj.type == "MESH"]
    for obj in created:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)  # templates stay out of the scene
    _library[key] = parts
    return parts


def place(folder, model, name, location, heading_degrees=0.0, scale=TILE, props=None):
    """Places a model at `location` (meters) turned by `heading_degrees` around Z."""
    parts = load(folder, model)
    rotation = (0.0, 0.0, math.radians(heading_degrees))
    if len(parts) == 1 and parts[0][1] == Matrix.Identity(4):
        obj = parts[0][0].copy()  # linked copy: shares the mesh data
        obj.name = name
        bpy.context.collection.objects.link(obj)
        root = obj
    else:
        root = bpy.data.objects.new(name, None)
        bpy.context.collection.objects.link(root)
        for part, matrix in parts:
            child = part.copy()
            child.name = part.name  # Blender suffixes duplicates (.001); scripts match by prefix
            bpy.context.collection.objects.link(child)
            child.parent = root
            child.matrix_parent_inverse = Matrix.Identity(4)
            child.matrix_basis = matrix
    root.location = location
    root.rotation_euler = rotation
    root.scale = (scale, scale, scale)
    for key, value in (props or {}).items():
        root[key] = value
    return root


def tiles(x, y, z=0.0):
    return (x * TILE, y * TILE, z * TILE)


# --- Layout ------------------------------------------------------------------------------------

def build_ground():
    half_x = (STREET_HALF_LENGTH + GROUND_MARGIN) * TILE
    half_y = (AVENUE_HALF_LENGTH + GROUND_MARGIN) * TILE
    kit.box("Ground", (0.0, 0.0, -GROUND_THICKNESS / 2), (half_x * 2, half_y * 2, GROUND_THICKNESS),
            mat=kit.material("Grass", GRASS, roughness=0.95))


def build_roads():
    for y in range(-AVENUE_HALF_LENGTH, AVENUE_HALF_LENGTH + 1):
        if y == 0:
            place(ROADS, "road-crossroad", "Crossroad", tiles(0, 0))
            continue
        model = "road-crossing" if abs(y) == 1 else "road-straight"
        place(ROADS, model, f"Avenue{y:+d}", tiles(0, y), QUARTER_TURN)
    for x in range(-STREET_HALF_LENGTH, STREET_HALF_LENGTH + 1):
        if x == 0:
            continue
        model = "road-crossing" if abs(x) == 1 else "road-straight"
        place(ROADS, model, f"Street{x:+d}", tiles(x, 0))


def build_blocks(rng):
    lamp = 0
    for sx in (-1, 1):
        for sy in (-1, 1):
            for bx in range(1, BLOCK_DEPTH + 2):
                for by in range(1, BLOCK_DEPTH + 2):
                    x, y = sx * bx, sy * by
                    if by > AVENUE_HALF_LENGTH or bx > STREET_HALF_LENGTH:
                        continue
                    place(ROADS, "tile-low", f"Pavement_{x:+d}_{y:+d}", tiles(x, y))
                    on_verge = bx == VERGE or by == VERGE
                    if on_verge:
                        lamp += 1
                        if lamp % LAMP_SPACING == 0:
                            # Lamps face the nearest road.
                            heading = QUARTER_TURN if bx == VERGE else 0.0
                            place(ROADS, "light-square-double", f"Lamp_{x:+d}_{y:+d}", tiles(x, y), heading)
                        else:
                            place(NATURE, rng.choice(TREES), f"Tree_{x:+d}_{y:+d}", tiles(x, y),
                                  rng.uniform(0.0, 360.0), TREE_SCALE)
                    elif (bx, by) in PARK_TILES:
                        place(NATURE, rng.choice(TREES), f"ParkTree_{x:+d}_{y:+d}", tiles(x, y),
                              rng.uniform(0.0, 360.0), TREE_SCALE)
                        place(NATURE, rng.choice(BUSHES), f"Bush_{x:+d}_{y:+d}",
                              tiles(x + 0.3 * sx, y - 0.3 * sy), rng.uniform(0.0, 360.0), BUSH_SCALE)
                    else:
                        # Buildings face the avenue or the street, whichever is closer.
                        facing_avenue = bx <= by
                        heading = (QUARTER_TURN if sx > 0 else -QUARTER_TURN) if facing_avenue else (
                            HALF_TURN if sy > 0 else 0.0)
                        place(COMMERCIAL, rng.choice(BUILDINGS), f"Building_{x:+d}_{y:+d}", tiles(x, y), heading)


def build_traffic_lights():
    corners = [(-0.5, -0.5, 0.0), (0.5, -0.5, QUARTER_TURN), (0.5, 0.5, HALF_TURN), (-0.5, 0.5, -QUARTER_TURN)]
    for index, (x, y, heading) in enumerate(corners, start=1):
        place(ROADS, "traffic-light", f"TrafficLight{index}", tiles(x, y), heading)
        kit.point_light(f"CornerLight{index}", (x * TILE, y * TILE, CORNER_LIGHT_HEIGHT), WARM_LIGHT,
                        CORNER_LIGHT_WATTS)
    kit.empty("TrafficController", (0.0, 0.0, 0.0),
              props={"script": "traffic_controller.rb", "cycle": TRAFFIC_CYCLE_SECONDS})


def car_heading(direction_x, direction_y):
    """Car models face Blender -Y; turn them to face (direction_x, direction_y)."""
    return math.degrees(math.atan2(direction_x, -direction_y))


def build_cars():
    for index, (model, axis, direction, start) in enumerate(TRAFFIC, start=1):
        if axis == "avenue":
            dx, dy = 0, direction
            x, y = LANE_OFFSET * direction, start  # right-hand traffic
        else:
            dx, dy = direction, 0
            x, y = start, -LANE_OFFSET * direction
        place(CARS, model, f"Car{index}", tiles(x, y, CAR_HEIGHT), car_heading(dx, dy), CAR_SCALE,
              props={"script": "traffic_car.rb", "tag": "car", "axis": axis, "dir_x": float(dx),
                     "dir_y": float(dy), "speed": CAR_SPEED,
                     "half_length": float((AVENUE_HALF_LENGTH if axis == "avenue" else STREET_HALF_LENGTH) * TILE)})
    for index, (model, x, y, heading) in enumerate(PARKED, start=1):
        place(CARS, model, f"ParkedCar{index}", tiles(x, y, CAR_HEIGHT), heading, CAR_SCALE)


def build_world():
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET, SUN_STRENGTH)
    kit.camera("Camera", CAMERA_LOCATION, (0.0, 0.0, 0.0), props={"script": "orbit_camera.rb", **CAMERA_PROPS})
    kit.empty("Caption", (0.0, 0.0, 0.0), props={"script": "caption.rb"})


def main():
    out_dir = kit.output_path_from_argv("games/city")
    if not os.path.isdir(ROADS):
        raise SystemExit("Kenney packs missing: run tools/assets/fetch_kenney.sh first")
    kit.reset_scene()
    rng = random.Random(SEED)
    build_ground()
    build_roads()
    build_blocks(rng)
    build_traffic_lights()
    build_cars()
    build_world()
    os.makedirs(out_dir, exist_ok=True)
    # The Kenney textures (CC0) travel inside the .blend, referenced by relative paths: the file
    # then opens anywhere and does not record where this checkout lives.
    blend = os.path.join(out_dir, "city.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    bpy.ops.file.pack_all()
    for image in bpy.data.images:  # packed: the original locations are no longer needed
        if image.packed_file is None:
            continue
        neutral = "//textures/" + image.name
        image.filepath = neutral
        for packed in image.packed_files:
            packed.filepath = neutral
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    kit.export_glb(os.path.join(out_dir, "scene.glb"))


main()
