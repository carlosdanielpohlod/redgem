"""Builds the city demo from CC0 Kenney packs: games/city/city.blend + scene.glb.

    tools/assets/fetch_kenney.sh                     # once: downloads the packs into .deps/assets
    tools/blender/run.sh build_city.py games/city

An avenue (along Blender Y) crosses a street (along X). Four blocks around the crossroad hold
buildings, tree verges and small parks. Moving cars carry traffic_car.rb; a TrafficController
switches the lights; the camera orbits the crossroad.

Kenney models are 1 unit per road tile; everything is laid out in tile units and scaled by TILE.
"""

import itertools
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

QUARTER_TURN = 90.0        # headings are in degrees
HALF_TURN = 180.0
FULL_TURN = 360.0
HALF = 0.5
BOTH_SIDES = 2             # a half-extent counts once on each side of the center
TRANSFORM_SIZE = 4         # rows and columns of a placement matrix
ORIGIN = (0.0, 0.0, 0.0)
AVENUE_HALF_LENGTH = 6     # tiles from the crossroad to each end of the avenue
STREET_HALF_LENGTH = 5
BLOCK_DEPTH = 4            # tiles of buildings behind each verge
LANE_OFFSET = 0.22         # tiles from the road center line to a lane center
CAR_HEIGHT = 0.02          # tiles: road surface thickness
VERGE = 1                  # first tile next to a road: trees and lamps
PARK_TILES = {(3, 3), (3, 4), (4, 3)}  # (x, y) block tiles left as parks in each block
BLOCK_SIDES = (-1, 1)      # the blocks lie on both sides of the avenue and of the street
BLOCK_TILES = range(VERGE, VERGE + BLOCK_DEPTH + 1)  # tiles from a road, across a block
BUSH_OFFSET = 0.3          # tiles from a park tree to its bush

GRASS = (0.12, 0.3, 0.06)
GROUND_THICKNESS = 0.5
GROUND_MARGIN = 2          # tiles of grass around the blocks
GRASS_ROUGHNESS = 0.95

# Kenney model names.
CROSSROAD = "road-crossroad"
CROSSING = "road-crossing"  # the tile with a zebra crossing, next to the crossroad
STRAIGHT = "road-straight"
PAVEMENT = "tile-low"
LAMP = "light-square-double"
TRAFFIC_LIGHT = "traffic-light"
CROSSING_DISTANCE = 1      # tiles from the crossroad to the zebra crossings
# The two roads, as the traffic scripts name them.
AVENUE = "avenue"
STREET = "street"
ROAD_HALF_LENGTH = {AVENUE: AVENUE_HALF_LENGTH, STREET: STREET_HALF_LENGTH}

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
# Traffic lights at the corners of the crossroad: (x tile, y tile, heading degrees).
LIGHT_CORNERS = [(-0.5, -0.5, 0.0), (0.5, -0.5, QUARTER_TURN), (0.5, 0.5, HALF_TURN), (-0.5, 0.5, -QUARTER_TURN)]
TEXTURE_FOLDER = "//textures/"  # where the packed textures say they live, relative to the .blend


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


def is_single_mesh(parts):
    """One mesh sitting exactly at the model's origin: it can be copied as it is."""
    return len(parts) == 1 and parts[0][1] == Matrix.Identity(TRANSFORM_SIZE)


def copy_single(parts, name):
    obj = parts[0][0].copy()  # linked copy: shares the mesh data
    obj.name = name
    bpy.context.collection.objects.link(obj)
    return obj


def copy_group(parts, name):
    """Copies of every part under a new empty called `name`."""
    root = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(root)
    for part, matrix in parts:
        child = part.copy()
        child.name = part.name  # Blender suffixes duplicates (.001); scripts match by prefix
        bpy.context.collection.objects.link(child)
        child.parent = root
        child.matrix_parent_inverse = Matrix.Identity(TRANSFORM_SIZE)
        child.matrix_basis = matrix
    return root


def place(folder, model, name, location, *, heading_degrees=0.0, scale=TILE, props=None):
    """Places a model at `location` (meters) turned by `heading_degrees` around Z."""
    parts = load(folder, model)
    rotation = (0.0, 0.0, math.radians(heading_degrees))
    root = copy_single(parts, name) if is_single_mesh(parts) else copy_group(parts, name)
    root.location = location
    root.rotation_euler = rotation
    root.scale = (scale, scale, scale)
    for key, value in (props or {}).items():
        root[key] = value
    return root


def tiles(x, y, z=0.0):
    return (x * TILE, y * TILE, z * TILE)


def tile_name(prefix, x, y):
    return f"{prefix}_{x:+d}_{y:+d}"


def random_turn(rng):
    return rng.uniform(0.0, FULL_TURN)


# --- Layout ------------------------------------------------------------------------------------

def build_ground():
    half_x = (STREET_HALF_LENGTH + GROUND_MARGIN) * TILE
    half_y = (AVENUE_HALF_LENGTH + GROUND_MARGIN) * TILE
    size = (half_x * BOTH_SIDES, half_y * BOTH_SIDES, GROUND_THICKNESS)
    kit.box("Ground", (0.0, 0.0, -GROUND_THICKNESS * HALF), size,
            mat=kit.material("Grass", GRASS, roughness=GRASS_ROUGHNESS))


def road_model(distance):
    """The road tile `distance` tiles away from the crossroad."""
    return CROSSING if abs(distance) == CROSSING_DISTANCE else STRAIGHT


def build_avenue():
    for y in range(-AVENUE_HALF_LENGTH, AVENUE_HALF_LENGTH + 1):
        if y == 0:
            place(ROADS, CROSSROAD, "Crossroad", tiles(0, 0))
            continue
        place(ROADS, road_model(y), f"Avenue{y:+d}", tiles(0, y), heading_degrees=QUARTER_TURN)


def build_street():
    for x in range(-STREET_HALF_LENGTH, STREET_HALF_LENGTH + 1):
        if x != 0:
            place(ROADS, road_model(x), f"Street{x:+d}", tiles(x, 0))


def build_roads():
    build_avenue()
    build_street()


def block_tiles():
    """Every tile of the four blocks, as ((side of the avenue, side of the street), (tiles from the
    avenue, tiles from the street)); the sides are -1 or 1."""
    for sx, sy, bx, by in itertools.product(BLOCK_SIDES, BLOCK_SIDES, BLOCK_TILES, BLOCK_TILES):
        if by <= AVENUE_HALF_LENGTH and bx <= STREET_HALF_LENGTH:
            yield (sx, sy), (bx, by)


def build_verge_tile(rng, x, y, lamp_heading):
    """A lamp when `lamp_heading` is given, a tree otherwise."""
    if lamp_heading is not None:
        place(ROADS, LAMP, tile_name("Lamp", x, y), tiles(x, y), heading_degrees=lamp_heading)
        return
    place(NATURE, rng.choice(TREES), tile_name("Tree", x, y), tiles(x, y), heading_degrees=random_turn(rng),
          scale=TREE_SCALE)


def build_park_tile(rng, x, y, side):
    sx, sy = side
    place(NATURE, rng.choice(TREES), tile_name("ParkTree", x, y), tiles(x, y), heading_degrees=random_turn(rng),
          scale=TREE_SCALE)
    place(NATURE, rng.choice(BUSHES), tile_name("Bush", x, y), tiles(x + BUSH_OFFSET * sx, y - BUSH_OFFSET * sy),
          heading_degrees=random_turn(rng), scale=BUSH_SCALE)


def building_heading(side, depth):
    """Buildings face the avenue or the street, whichever is closer."""
    sx, sy = side
    bx, by = depth
    if bx <= by:
        return QUARTER_TURN if sx > 0 else -QUARTER_TURN
    return HALF_TURN if sy > 0 else 0.0


def build_blocks(rng):
    verge_tiles = 0
    for side, depth in block_tiles():
        x, y = side[0] * depth[0], side[1] * depth[1]
        place(ROADS, PAVEMENT, tile_name("Pavement", x, y), tiles(x, y))
        if VERGE in depth:
            verge_tiles += 1
            # Lamps face the nearest road.
            facing = QUARTER_TURN if depth[0] == VERGE else 0.0
            build_verge_tile(rng, x, y, facing if verge_tiles % LAMP_SPACING == 0 else None)
        elif depth in PARK_TILES:
            build_park_tile(rng, x, y, side)
        else:
            place(COMMERCIAL, rng.choice(BUILDINGS), tile_name("Building", x, y), tiles(x, y),
                  heading_degrees=building_heading(side, depth))


def build_traffic_lights():
    for index, (x, y, heading) in enumerate(LIGHT_CORNERS, start=1):
        place(ROADS, TRAFFIC_LIGHT, f"TrafficLight{index}", tiles(x, y), heading_degrees=heading)
        kit.point_light(f"CornerLight{index}", (x * TILE, y * TILE, CORNER_LIGHT_HEIGHT), WARM_LIGHT,
                        CORNER_LIGHT_WATTS)
    kit.empty("TrafficController", ORIGIN, props={"script": "traffic_controller.rb", "cycle": TRAFFIC_CYCLE_SECONDS})


def car_heading(direction_x, direction_y):
    """Car models face Blender -Y; turn them to face (direction_x, direction_y)."""
    return math.degrees(math.atan2(direction_x, -direction_y))


def lane(axis, direction, start):
    """((direction x, direction y), (x tile, y tile)) of a car on the right-hand lane of a road."""
    if axis == AVENUE:
        return (0, direction), (LANE_OFFSET * direction, start)
    return (direction, 0), (start, -LANE_OFFSET * direction)


def build_traffic():
    for index, (model, axis, direction, start) in enumerate(TRAFFIC, start=1):
        (dx, dy), (x, y) = lane(axis, direction, start)
        place(CARS, model, f"Car{index}", tiles(x, y, CAR_HEIGHT), heading_degrees=car_heading(dx, dy),
              scale=CAR_SCALE,
              props={"script": "traffic_car.rb", "tag": "car", "axis": axis, "dir_x": float(dx),
                     "dir_y": float(dy), "speed": CAR_SPEED, "half_length": float(ROAD_HALF_LENGTH[axis] * TILE)})


def build_parked_cars():
    for index, (model, x, y, heading) in enumerate(PARKED, start=1):
        place(CARS, model, f"ParkedCar{index}", tiles(x, y, CAR_HEIGHT), heading_degrees=heading, scale=CAR_SCALE)


def build_cars():
    build_traffic()
    build_parked_cars()


def build_world():
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET, SUN_STRENGTH)
    kit.camera("Camera", CAMERA_LOCATION, ORIGIN, props={"script": "orbit_camera.rb", **CAMERA_PROPS})
    kit.empty("Caption", ORIGIN, props={"script": "caption.rb"})


def save_blend(blend):
    """The Kenney textures (CC0) travel inside the .blend, referenced by relative paths: the file
    then opens anywhere and does not record where this checkout lives."""
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    bpy.ops.file.pack_all()
    for image in bpy.data.images:  # packed: the original locations are no longer needed
        if image.packed_file is None:
            continue
        neutral = TEXTURE_FOLDER + image.name
        image.filepath = neutral
        for packed in image.packed_files:
            packed.filepath = neutral
    bpy.ops.wm.save_as_mainfile(filepath=blend)


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
    save_blend(os.path.join(out_dir, "city.blend"))
    kit.export_glb(os.path.join(out_dir, "scene.glb"))


main()
