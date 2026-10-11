"""Photoreal evening street for promotional renders (Cycles): CC0 scanned street assets, an
unbranded car and scanned people.

    python3 -I tools/assets/fetch_showcase.py              # once: Poly Haven + ambientCG assets (CC0)
    python3 -I tools/assets/fetch_ad_assets.py             # once: car and people (BlenderKit)
    tools/blender/run.sh build_showcase.py showcase        # builds showcase/street.blend and renders

Optional environment variables:
  SHOWCASE_PREVIEW=1   quick low-sample preview render
  SHOWCASE_RENDER=0    skip the Cycles render
  SHOWCASE_EXPORT=0    skip exporting the real-time version to games/showcase (opened by the editor)
  SHOWCASE_SHOT=ad     which camera: "ad" (car and people, default), "hero" (the car), "street"
The render needs an NVIDIA RTX card (Cycles with OptiX) and the export about 7 GB of memory:
close the editor first, and consider BLENDER_MEMORY_MAX=8500M (see run.sh).
Everything is in meters, Blender axes: the street runs along +Y, the camera looks down it toward
the sunset.
"""

import functools
import glob
import itertools
import math
import os
import random
import sys
from typing import NamedTuple

import bpy
from mathutils import Vector, noise

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402
import walk_rig  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(REPO_ROOT, ".deps", "assets", "showcase")
MODELS = os.path.join(ASSETS, "models")
TEXTURES = os.path.join(ASSETS, "textures")
HDRI = os.path.join(ASSETS, "hdri", "belfast_sunset_puresky_4k.hdr")

SEED = 11
PREVIEW = os.environ.get("SHOWCASE_PREVIEW") == "1"
RENDER = os.environ.get("SHOWCASE_RENDER", "1") == "1"
EXPORT = os.environ.get("SHOWCASE_EXPORT", "1") == "1"
GAME_DIR = os.path.join(REPO_ROOT, "games", "showcase")

HALF = 0.5                         # centers and half-extents are half a size
BOTH_SIDES = 2                     # a margin or a radius counts once on each side
SIDES = (-1, 1)                    # the left-hand and the right-hand side of the street
QUARTER_TURN = 90.0                # degrees
ORIGIN = (0.0, 0.0, 0.0)
X_AXIS = 0                         # indices into an (x, y, z)
Z_AXIS = 2
# Names Blender gives to things this script asks for again and again.
PRINCIPLED = kit.PRINCIPLED_BSDF
MESH = "MESH"                      # object type
DECIMATE = "DECIMATE"              # the modifier that removes triangles
SUBSURF = "SUBSURF"                # the modifier that subdivides them
SRGB = "sRGB"                      # color space of maps that hold colors
NON_COLOR = "Non-Color"            # ... and of maps that hold numbers (normals, roughness)
FLAT = "FLAT"                      # texture projected along one axis
BOX = "BOX"                        # ... or along all three, for blocks without a UV map
# Maps that go straight into one input of the shader: (key in the maps, color space, input).
PLAIN_MAPS = (("roughness", NON_COLOR, "Roughness"), ("metal", NON_COLOR, "Metallic"),
              ("emission", SRGB, "Emission Color"))
# Custom properties the engine reads.
TINT = "tint"                      # color (sRGB hex) the object is multiplied by
VISIBLE_FROM = "visible_from"      # drawn only between these distances from the camera (meters)
VISIBLE_TO = "visible_to"

# --- Real-time export (the engine has no path tracing, volumes or procedural textures) ---------
# Share of their triangles the scanned models keep (they are made for close-ups; a street has
# dozens of each).
ENGINE_DECIMATE = {
    "island_tree_02": 0.08,       # of ~1M
    "fire_hydrant": 0.3,
    "concrete_road_barrier": 0.3,
    "potted_plant_02": 0.35,
}
# The car comes in three levels of detail. Up close it has one level of subdivision (2.8M
# triangles); further away, none (a quarter of that, faceted only from near); far away, a fraction
# of that. (near until, middle until) in meters from the camera.
ENGINE_CAR_DETAIL_RANGES = (14.0, 45.0)
ENGINE_CAR_FAR_KEEP = 0.15         # share of the unsubdivided car's triangles kept far away
PARKED_TAG = "parked"
ENGINE_SUN_DISTANCE = 100.0
ENGINE_SUN_HEIGHT = 14.0
ENGINE_SUN_STRENGTH = 3.0
# This script only builds the scene: what is there and where. What it all does in the game (who
# drives, who walks, the falling leaves) is Ruby: games/showcase/scripts/main_scene.rb.
# The car model's nose points along -Y in Blender, which is +Z in glTF: the opposite of the
# engine's forward (-Z).
ENGINE_CAR_HEADING = 180.0         # degrees: down the avenue, toward the sunset
# The car (millions of triangles, 8K textures) and the scanned people are made for offline renders;
# a laptop GPU needs far less.
ENGINE_MAX_TEXTURE = 2048          # pixels on the longer side
ENGINE_CAR_TRIANGLES = 3_200_000   # the whole model with one level of subdivision: smooth panels
ENGINE_PERSON_TRIANGLES = 120_000
ENGINE_SUBSURF_LEVELS = 1
# The paint is exported near-white and colored per car with the engine's `tint` property, so
# every car shares the same meshes.
ENGINE_PAINT_PREFIXES = ("Car Paint", "1 car paint")
ENGINE_HERO_PAINT = "#8c0b12"
ENGINE_WHEEL_REACH = 1.08          # parts within this many tire radii of a wheel's center turn with it
ENGINE_WHEEL_HALF_WIDTH = 0.22     # ... and this close to it sideways (meters)
ENGINE_LANE = 1.2                  # distance of a lane's center from the middle of the road: the lanes
                                   # are narrow, cars park along both curbs
ENGINE_PARKING = 3.55              # ... and of a car parked at the curb
# Other cars: (name, x, y, heading degrees (0 = toward the start of the street), paint). The ones
# named "Parked..." get the tag "parked"; which of the others drive, and how fast, is the game's
# business (main_scene.rb).
ENGINE_TRAFFIC = [
    ("TrafficBlue", -ENGINE_LANE, 70.0, 0.0, "#1c3a7a"),
    ("TrafficWhite", -ENGINE_LANE, 150.0, 0.0, "#d6d6d0"),
    ("TrafficBeige", -ENGINE_LANE, 112.0, 0.0, "#b9a98a"),
    ("TrafficGrey", ENGINE_LANE, 75.0, 180.0, "#35383c"),
    ("TrafficGreen", ENGINE_LANE, 140.0, 180.0, "#1f4d3a"),
    ("ParkedBlack", -ENGINE_PARKING, 24.0, 0.0, "#0c0c0e"),
    ("ParkedWine", -ENGINE_PARKING, 58.0, 0.0, "#4a1220"),
    ("ParkedSand", -ENGINE_PARKING, 64.5, 0.0, "#c2b59b"),
    ("ParkedTeal", -ENGINE_PARKING, 131.0, 0.0, "#1d5560"),
    ("ParkedGraphite", -ENGINE_PARKING, 172.0, 0.0, "#2b2d30"),
    ("ParkedSilver", ENGINE_PARKING, 118.0, 180.0, "#9a9da1"),
    ("ParkedWhite", ENGINE_PARKING, 47.0, 180.0, "#e3e3de"),
    ("ParkedNavy", ENGINE_PARKING, 86.0, 180.0, "#16264d"),
    ("ParkedOrange", ENGINE_PARKING, 92.5, 180.0, "#b5541c"),
    ("ParkedOlive", ENGINE_PARKING, 158.0, 180.0, "#4b5230"),
]
PARKED_PREFIX = "Parked"
ENGINE_DECIMATE_MIN_TRIANGLES = 300  # tiny parts are left alone (decimating them only damages them)
TRIANGLE_CORNERS = 3
# The car's materials are procedural node setups that glTF cannot carry: plain values for the
# engine, by material name prefix -> (base color, metallic, roughness).
ENGINE_CAR_MATERIALS = {
    "Car Paint": ((0.9, 0.9, 0.9), 0.55, 0.22),
    "1 car paint": ((0.9, 0.9, 0.9), 0.55, 0.22),
    "Black plastic": ((0.03, 0.03, 0.03), 0.0, 0.55),
    "black rubber": ((0.02, 0.02, 0.02), 0.0, 0.9),
    "Rim paint": ((0.62, 0.62, 0.64), 1.0, 0.3),
    "Rim accent": ((0.1, 0.1, 0.1), 1.0, 0.35),
    "Grey paint": ((0.22, 0.22, 0.23), 0.5, 0.5),
    "Glass": ((0.03, 0.035, 0.04), 0.0, 0.05),
    "Dark glass": ((0.02, 0.022, 0.026), 0.0, 0.05),
    "Windshield Glass": ((0.03, 0.035, 0.04), 0.0, 0.05),
    "Clear Glass": ((0.03, 0.035, 0.04), 0.0, 0.05),
    "Clear Green glass": ((0.03, 0.04, 0.035), 0.0, 0.05),
    "Sunroof": ((0.02, 0.022, 0.026), 0.0, 0.05),
    "chrome": ((0.8, 0.8, 0.8), 1.0, 0.15),
    "Chrome Smooth": ((0.8, 0.8, 0.8), 1.0, 0.1),
    "Mirror": ((0.85, 0.85, 0.85), 1.0, 0.03),
}
# Lamps of the car that glow in the engine: material name prefix -> (color, emission strength).
ENGINE_CAR_GLOW = {
    "DRL Emission": ((1.0, 0.95, 0.85), 9.0),
    "Carlight triangle": ((1.0, 0.93, 0.8), 6.0),
    "Red Glass": ((1.0, 0.04, 0.02), 3.0),
    "Orange Glass": ((1.0, 0.35, 0.03), 1.5),
}
# Real lights carried by the car, in its own space (x right, y back, z up; meters): position,
# color, watts. Two headlights and one tail glow.
ENGINE_CAR_LIGHTS = [
    ("HeadlightL", (-0.72, -4.2, 0.62), (1.0, 0.93, 0.8), 2200.0),
    ("HeadlightR", (0.72, -4.2, 0.62), (1.0, 0.93, 0.8), 2200.0),
    ("TailGlow", (0.0, 3.6, 0.6), (1.0, 0.08, 0.04), 500.0),
]
# The engine's lights fall off faster than Cycles' over a street's distances.
ENGINE_LAMP_WATTS = 14000.0
ENGINE_SUN_COLOR = (1.0, 0.84, 0.68)   # late afternoon: warm, not orange
ENGINE_PERSON_ROUGHNESS = 0.8          # skin and cloth
# Worn asphalt: the road becomes a grid whose points sink a little, in broad waves and in a few
# deeper dips (never above the original surface, so the car's wheels do not sink into it).
ENGINE_ROAD_CELL = 0.25                # meters between grid points
ENGINE_ROAD_WAVE_DEPTH = 0.035         # meters
ENGINE_ROAD_WAVE_SIZE = 9.0            # meters per wave
ENGINE_ROAD_DIP_DEPTH = 0.05
ENGINE_ROAD_DIP_SIZE = 2.2
ENGINE_ROAD_DIP_THRESHOLD = 0.62       # share of the noise range below which there is no dip
ENGINE_ROAD_CROWN = 0.05               # the middle of the road is this much higher than its edges
ENGINE_ROAD_CROWN_POWER = 2            # ... falling off as a parabola
# Potholes: (x, y, radius, depth) in meters, with a ragged edge. Each also leaves a marker object
# in the scene (tag "pothole", with its radius and depth), which is how the game knows of them.
ENGINE_POTHOLES = [
    (2.6, 34.0, 0.75, 0.11), (1.5, 58.0, 0.6, 0.09), (-1.9, 47.0, 0.9, 0.12), (2.9, 92.0, 0.8, 0.1),
    (-2.6, 110.0, 0.7, 0.1), (1.9, 133.0, 1.0, 0.13), (-1.4, 161.0, 0.65, 0.09), (2.4, 176.0, 0.85, 0.11),
]
ENGINE_POTHOLE_EDGE_NOISE = 0.25       # how irregular the outline is (fraction of the radius)
ENGINE_POTHOLE_EDGE_SCALE = 0.35       # meters per wobble of the outline
ENGINE_POTHOLE_WALL_STEEPNESS = 3.0    # the bottom is flat over the inner (1 - 1/this) of the radius
# Each noise pattern reads a different slice (z) of the same 3D noise, so they do not line up.
NOISE_LAYER_DIPS = 7.0
NOISE_LAYER_POTHOLES = 3.0
# Falling leaves: one small mesh (a leaf 15 cm long, folded along its midrib), tinted per template.
LEAF_POINTS = [(0.0, -0.075, 0.0), (0.042, 0.0, 0.011), (0.0, 0.075, 0.0), (-0.042, 0.0, 0.011), (0.0, 0.0, -0.006)]
LEAF_FACES = [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]
LEAF_BASE_COLOR = (0.9, 0.9, 0.9)
LEAF_ROUGHNESS = 0.6
LEAF_COLORS = ["#c9731e", "#b08a22", "#8a4a1c", "#93a030", "#d49a2a"]  # autumn, with some green left
# The sunset, as the engine's environment object reads it (colors are sRGB hex).
ENGINE_ENVIRONMENT = {
    "environment": True,
    "sky_top": "#5d6f9e",
    "sky_horizon": "#f3cdb3",
    "ground_color": "#4d4542",
    "fog_color": "#dcc7ba",
    "fog_density": 0.0018,
    "ambient": 0.9,
    "exposure": 1.0,
    "bloom": 0.32,
    "vignette": 0.42,
    "sun_glow": 0.7,
    "wind": 0.7,
    # The grade of a racing game's dusk: punchier, a little more color, grain and lens fringes.
    "contrast": 1.12,
    "saturation": 1.14,
    "grain": 0.45,
    "aberration": 0.7,
}
# The people who walk in the game get legs that walk, and stand along the sidewalk: person
# collection name -> (heading in degrees, 0 = toward the start of the street; x in meters).
ENGINE_WALKERS = {
    "Wenceslavus": (180.0, 7.4),
    "Luke": (0.0, 7.9),
    "Standing Survivor Girl Green Top Cowboy": (0.0, -7.4),
    "Axel Standing": (180.0, -7.9),
}
POTHOLE_TAG = "pothole"
AXES = 3
MIN_TO_JOIN = 2                        # joining takes at least two objects
ALBEDO_WORD = "albedo"                 # in the name of a scan's plain color texture

# --- Street layout (meters) -------------------------------------------------------------------
STREET_LENGTH = 220.0
STREET_START = -30.0
ROAD_WIDTH = 9.0
ROAD_TILE_LENGTH = 8.0            # Road007 repeats its dashed center line every tile
SIDEWALK_WIDTH = 4.5
CURB_WIDTH = 0.3
CURB_HEIGHT = 0.15
SIDEWALK_TILE = 2.0               # meters per concrete_pavement texture repeat
CURB_TILE = 1.0
LAMP_SPACING = 18.0
TREE_OFFSET = 9.0                 # trees sit halfway between lamps
LAMP_FROM_CURB = 0.6
LAMP_ARM_REACH = 0.4              # the lantern hangs this far from the post, toward the road
TREE_SCALE_SPREAD = (0.9, 1.1)    # trees differ a little in size
FULL_TURN_DEGREES = 360.0
BOX_PROJECTION_BLEND = 0.2        # softness of the seams of box-projected textures
RGBA_CHANNELS = 4
ROAD_UNDERSIDE = -0.1             # slabs go this far below the surface (meters)
TREE_FROM_CURB = 1.3
TREE_SCALE = 1.7                  # the scanned tree is 3.4 m; street trees are taller
LAMP_HEAD_HEIGHT = 3.7
LAMP_LIGHT_WATTS = 220.0
LAMP_LIGHT_RADIUS = 0.12
LAMP_COLOR_TEMPERATURE = (1.0, 0.62, 0.32)
BULB_EMISSION = 18.0
STREET_END = STREET_START + STREET_LENGTH
CURB_LINE = ROAD_WIDTH * HALF + CURB_WIDTH   # distance from the middle of the road to the sidewalk
FACADE_LINE = CURB_LINE + SIDEWALK_WIDTH     # ... and to the front of the buildings
# Meters covered by one texture repeat along (x, y, z).
ROAD_TILING = (ROAD_WIDTH, ROAD_TILE_LENGTH, 1.0)
SIDEWALK_TILING = (SIDEWALK_TILE, SIDEWALK_TILE, 1.0)
CURB_TILING = (CURB_TILE, CURB_TILE, CURB_TILE)
ROAD_TEXTURE = "Road007"
SIDEWALK_TEXTURE = "concrete_pavement"
CONCRETE_TEXTURE = "concrete_floor_worn_001"  # curbs, plinths and cornices
# Names of objects that are looked up again later.
ROAD_NAME = "Road"
HAZE_NAME = "Haze"
BUILDING_PREFIX = "Building"
LAMP_LIGHT_PREFIX = "LampLight"
# Scanned models.
LAMP_MODEL = "street_lamp_01"
LAMP_GLASS_WORDS = ("glass", "bulb", "light")  # in the names of the lamp's materials that glow
STREET_TREE = "island_tree_02"
JACARANDA = "jacaranda_tree"
MANHOLE = "water_manhole_cover"
# The worn road's grid for the real-time export (see ENGINE_ROAD_CELL).
ENGINE_ROAD_COLUMNS = round(ROAD_WIDTH / ENGINE_ROAD_CELL)
ENGINE_ROAD_ROWS = round(STREET_LENGTH / ENGINE_ROAD_CELL)

BUILDING_MIN_WIDTH = 9.0
BUILDING_MAX_WIDTH = 17.0
BUILDING_GAP = 0.0                # a continuous street front
BUILDING_DEPTH = 14.0
BUILDING_MIN_HEIGHT = 11.0
BUILDING_MAX_HEIGHT = 34.0
TOWER_CHANCE = 0.18               # some tall glass office buildings
TOWER_MIN_HEIGHT = 40.0
TOWER_MAX_HEIGHT = 70.0
BRICK_FACADE_TILE = 12.0          # meters covered by one repeat of the brick facades
GLASS_FACADE_TILE = 24.0
PLINTH_HEIGHT = 0.6
PLINTH_PROTRUSION = 0.05
CORNICE_HEIGHT = 0.5
CORNICE_OVERHANG = 0.35
WINDOW_GLOW_MIN = 0.0
WINDOW_GLOW_MAX = 6.0
BRICK_FACADES = ["Facade018A", "Facade020A"]
GLASS_FACADE = "Facade006"

# Wet asphalt: lower roughness everywhere, near-mirror puddles where noise is high.
WET_ROUGHNESS_FACTOR = 0.45
PUDDLE_ROUGHNESS = 0.04
PUDDLE_NOISE_SCALE = 0.18
PUDDLE_THRESHOLD = 0.58
PUDDLE_SOFTNESS = 0.04
PUDDLE_DARKEN = 0.6

# Shop fronts on the ground floor of the brick buildings: a painted band with lit windows, awnings,
# a neon strip, and sometimes a cafe table, a plant or a bench in front. All sizes in meters.
SHOP_SEED_OFFSET = 1               # their own random sequence: the buildings stay as they were
SHOP_BAND_TOP = 3.75
SHOP_BAND_DEPTH = 0.06
SHOP_BAY_WIDTH = 4.2               # a building has as many bays as fit
SHOP_BAY_MARGIN = 1.0              # wall left between two windows
SHOP_WINDOW_SILL = 0.35            # above the plinth
SHOP_WINDOW_HEIGHT = 2.0
SHOP_WINDOW_DEPTH = 0.05
SHOP_FRAME = 0.07
SHOP_FRONT_DEPTH = 0.18            # how far the whole shop front stands out of the wall
SHOP_MULLION_SPACING = 1.1
SHOP_BAND_COLORS = [(0.03, 0.09, 0.09), (0.16, 0.03, 0.03), (0.03, 0.04, 0.1), (0.05, 0.05, 0.05),
                    (0.09, 0.1, 0.04), (0.45, 0.4, 0.3), (0.02, 0.06, 0.03)]
SHOP_FRAME_COLOR = (0.02, 0.02, 0.02)
SHOP_LIT_CHANCE = 0.75             # the others are closed for the day: dark glass
SHOP_GLOWS = [(1.0, 0.62, 0.3), (1.0, 0.74, 0.45), (1.0, 0.5, 0.22), (0.6, 0.78, 1.0), (1.0, 0.68, 0.36)]
SHOP_GLOW_STRENGTH = (0.3, 0.7)    # more than this and the tone mapping washes them out to white
SHOP_DARK_GLASS = (0.02, 0.025, 0.03)
# What is on show, seen against the light: a counter along the window and a few shapes on it.
DISPLAY_COLOR = (0.015, 0.012, 0.01)
DISPLAY_COUNTER_HEIGHT = 0.45
DISPLAY_ITEMS = (2, 5)             # how many per window
DISPLAY_ITEM_WIDTH = (0.2, 0.55)
DISPLAY_ITEM_HEIGHT = (0.25, 1.0)
DISPLAY_DEPTH = 0.03
SHOP_GLASS_ROUGHNESS = 0.06
AWNING_CHANCE = 0.7
AWNING_REACH = 1.3
AWNING_THICKNESS = 0.05
AWNING_HEIGHT = 3.3                # where it meets the wall
AWNING_SLOPE_DEGREES = 18.0
AWNING_SIDE_MARGIN = 0.3
AWNING_COLORS = [(0.45, 0.04, 0.04), (0.04, 0.22, 0.1), (0.05, 0.1, 0.3), (0.6, 0.38, 0.05),
                 (0.5, 0.45, 0.36), (0.3, 0.07, 0.2), (0.05, 0.25, 0.28)]
AWNING_ROUGHNESS = 0.85
NEON_CHANCE = 0.45
NEON_COLORS = [(1.0, 0.1, 0.45), (0.1, 0.85, 1.0), (1.0, 0.6, 0.1), (0.3, 1.0, 0.4), (0.7, 0.3, 1.0)]
NEON_STRENGTH = 9.0
NEON_SIZE = (0.04, 0.5, 0.1)       # depth, share of the bay's width, height
NEON_HEIGHT = 3.55
CAFE_CHANCE = 0.3
CAFE_FROM_WALL = 0.55
CAFE_TABLE_SPACING = 2.1
PLANT_CHANCE = 0.25
PLANT_FROM_WALL = 0.45
BENCH_CHANCE = 0.3                 # in front of closed shops
BENCH_FROM_WALL = 0.4
SHOP_FRAME_ROUGHNESS = 0.4
SHOP_FRAME_METALLIC = 0.6
SHOP_BAND_ROUGHNESS = 0.6
DISPLAY_ROUGHNESS = 0.9
GLOW_STEPS = 2                     # the glow of shop windows comes in halves
MIN_MULLIONS = 2                   # a window has at least the bars at its two ends
SHOP_FLOOR = CURB_HEIGHT + PLINTH_HEIGHT   # where the shop fronts start, above the plinth
SHOP_WINDOW_Z = SHOP_FLOOR + SHOP_WINDOW_SILL + SHOP_WINDOW_HEIGHT / 2   # middle of the windows
SHOP_WINDOW_BOTTOM = SHOP_WINDOW_Z - SHOP_WINDOW_HEIGHT / 2
# The bars along a window's top and sill: (name, height).
SHOP_WINDOW_EDGES = (("top", SHOP_WINDOW_Z + SHOP_WINDOW_HEIGHT / 2), ("sill", SHOP_WINDOW_BOTTOM))
# How far from the wall the middle of each layer of a shop front is.
WINDOW_OUT = SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH / 2
DISPLAY_OUT = SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH + DISPLAY_DEPTH / 2
FRAME_OUT = SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH + SHOP_FRAME / 2
CAFE_TABLE_OFFSETS = (-CAFE_TABLE_SPACING / 2, CAFE_TABLE_SPACING / 2)  # two tables per cafe
CAFE_SET = "outdoor_table_chair_set_01"
BENCH = "painted_wooden_bench"
POTTED_PLANT = "potted_plant_02"


class Front(NamedTuple):
    """The street front of a building."""
    name: str
    side: int          # -1 on the left-hand side of the street, 1 on the right-hand
    x_near: float      # x of the wall
    start: float       # y where it starts
    width: float       # meters along the street


class Bay(NamedTuple):
    """One shop window's share of a front."""
    name: str
    side: int
    x_near: float
    center: float      # y of its middle
    width: float


# Things along the curb, down the whole street: (model, weight, rotation on the right-hand sidewalk;
# the left-hand one mirrors it). None is an empty spot.
SCATTER_SEED_OFFSET = 2
SCATTER_FROM = 50.0                # beyond the hand-placed props near the camera
SCATTER_SLOTS = (4.5, 13.5)        # meters after each lamp (the tree is at 9)
TRASH_PILE_SLOT = "trash_pile"     # the scatter entry that stands for the whole TRASH_PILE
SCATTER = [
    (None, 3, 0.0), ("metal_trash_can", 3, 90.0), ("fire_hydrant", 2, 90.0), ("utility_box_01", 1, -90.0),
    ("utility_box_02", 1, -90.0), ("modular_street_seating", 2, -90.0), ("planter_box_01", 2, 90.0),
    ("planter_box_02", 2, 90.0), ("planter_box_03", 2, 90.0), (TRASH_PILE_SLOT, 2, 0.0),
]
TRASH_PILE = [("trashbag", (0.0, 0.0), 0.0), ("trashbag", (0.35, 0.3), 70.0), ("cardboard_box_01", (-0.2, 0.55), 25.0),
              ("plastic_crate_02", (0.3, -0.5), -15.0)]
MORE_MANHOLES = [(-1.8, 66.0), (2.4, 97.0), (-2.6, 128.0), (1.2, 163.0)]
# Some of the street's trees are jacarandas in bloom. That scan is a 19 m tree: scaled to the street.
JACARANDA_EVERY = 3
# Models too heavy to even keep in memory next to the rest of the scene are thinned as they are
# imported: model -> share of its triangles kept.
IMPORT_DECIMATE = {"jacaranda_tree": 0.03}  # of ~3.9M
JACARANDA_SCALE = 0.38

# Props near the camera: (model, x side sign, y, rotation degrees around Z).
PROPS = [
    ("fire_hydrant", 1, 6.5, 90.0),
    ("metal_trash_can", -1, 4.0, -90.0),
    ("utility_box_01", 1, 14.0, -90.0),
    ("modular_street_seating", -1, 22.0, 90.0),
    ("planter_box_01", 1, 26.0, 0.0),
    ("concrete_road_barrier", -1, 46.0, 90.0),
]
MANHOLES = [(-2.2, 12.0), (2.0, 31.0)]
PROP_FROM_CURB = 1.0

# --- Cast: an unbranded car and scanned people (BlenderKit, .deps/assets/ad) ----------------------
# Their licenses allow commercial renders. No real car brands and no clothing logos, so the
# renders can be used in advertising.
CAST_DIR = os.path.join(REPO_ROOT, ".deps", "assets", "ad")
SIDEWALK_TOP = CURB_HEIGHT  # people stand on the sidewalk
BUILDING_FRONT = ROAD_WIDTH / 2 + CURB_WIDTH + SIDEWALK_WIDTH - PLINTH_PROTRUSION  # right-hand facades
# (folder/blend name, collection name, location, heading degrees around Z). Models face -Y, so
# heading 0 faces the camera and -90 faces -X (toward the road, for the right-hand sidewalk).
# The car model's tires end 2.9 cm above its origin; sink it a bit more so they look loaded.
CAR_TIRE_LIFT = 0.04
CARS = [
    ("generic_sport_sedan", "Generic Sport Sedan 2022 PL", (3.0, 10.0, -CAR_TIRE_LIFT), 16.0),  # at the curb
]
PEOPLE = [
    ("wenceslavus", "Wenceslavus", (5.6, 8.8, SIDEWALK_TOP), -95.0),   # walking up to the car
    ("andrew", "Andrew", (6.5, 13.0, SIDEWALK_TOP), -15.0),            # on his phone
    ("charles", "Charles", (BUILDING_FRONT - SHOP_FRONT_DEPTH, 11.5, SIDEWALK_TOP), -90.0),  # leaning on a shop front
    ("luke", "Luke", (8.0, 25.0, SIDEWALK_TOP), -30.0),               # further down the sidewalk
    # (Scans by Calvin Serrano; outside the "ad" shot's frame, they populate the playable street.)
    ("melissa_green", "Standing Survivor Girl Green Top Cowboy", (-7.4, 34.0, SIDEWALK_TOP), 0.0),
    ("axel", "Axel Standing", (-8.3, 6.0, SIDEWALK_TOP), 180.0),
    # (Wide trousers: she stands.)
    ("melissa_white", "Standing Cute Girl White Jacket Jeans", (7.9, 66.0, SIDEWALK_TOP), -70.0),
]
HERO_CAR = "Generic Sport Sedan 2022 PL"
# How that model names its tires, and the parts at a wheel that do not turn with it.
CAR_TIRE_PREFIX = "4-PilotSport"
CAR_FIXED_PART_WORD = "caliper"

# --- Camera and render -----------------------------------------------------------------------
# Shots: "ad" frames the car with people around it; "hero" is the car alone in 3/4 front view;
# "street" looks down the avenue.
SHOT = os.environ.get("SHOWCASE_SHOT", "ad")
SHOTS = {
    "ad": {"location": (-0.3, 1.0, 0.95), "target": (4.2, 12.5, 1.2), "lens": 35.0, "fstop": 2.8},
    "hero": {"location": (4.1, -2.6, 0.8), "target": (1.5, 9.5, 0.9), "lens": 50.0, "fstop": 2.4},
    "street": {"location": (2.3, -6.0, 1.05), "target": (0.6, 40.0, 2.2), "lens": 35.0, "fstop": 2.8},
}
CAMERA_LOCATION = SHOTS[SHOT]["location"]
CAMERA_TARGET = SHOTS[SHOT]["target"]
LENS_MM = SHOTS[SHOT]["lens"]
SENSOR_MM = 36.0
F_STOP = SHOTS[SHOT]["fstop"]
FOCUS_DISTANCE = 13.0  # used when the shot has no hero car to focus on
CAR_FOCUS_HEIGHT = 0.7
# Soft photo-studio style fill from the camera side, so the backlit car is readable.
FILL_LIGHT_OFFSET = (-1.5, 2.5, 2.0)    # meters from the camera
FILL_LIGHT_SIZE = 4.0
FILL_LIGHT_WATTS = 450.0
FILL_LIGHT_COLOR = (1.0, 0.86, 0.72)
TILE_SIZE = 1024       # tiled rendering keeps GPU memory and heat down on a laptop
SUN_TARGET_AZIMUTH = math.radians(97.0)  # sun just right of straight down the street (+Y = 90 deg)
WORLD_STRENGTH = 1.0
HAZE_DENSITY = 0.006
HAZE_HEIGHT = 60.0
HAZE_ANISOTROPY = 0.6
RESOLUTION = (1920, 1080) if not PREVIEW else (960, 540)
SAMPLES = 384 if not PREVIEW else 96
NOISE_THRESHOLD = 0.01
MAX_BOUNCES = 8
EXPOSURE = 0.0
LOOK = "AgX - Medium High Contrast"
HDRI_ANALYSIS_WIDTH = 256
EQUIRECTANGULAR_ASPECT = 2      # a sky image is twice as wide as it is tall
RGB_CHANNELS = 3                # of the RGBA_CHANNELS each pixel has
SHOTS_WITH_CAR = ("hero", "ad")  # these focus on the car
GPU_BACKEND = "OPTIX"           # Cycles on NVIDIA RTX cards, also its denoiser
FULL_RESOLUTION = 100           # percent of RESOLUTION


# --- Materials ---------------------------------------------------------------------------------

def image_node(nodes, path, colorspace=SRGB, projection=FLAT):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(path, check_existing=True)
    node.image.colorspace_settings.name = colorspace
    node.projection = projection
    if projection == BOX:
        node.projection_blend = BOX_PROJECTION_BLEND
    return node


def world_mapping(nodes, links, scale, offset=ORIGIN):
    """Texture coordinates from world position, so tiling is in meters on any object. `scale` is
    the meters one repeat covers along (x, y, z)."""
    scale_x, scale_y, scale_z = scale
    geometry = nodes.new("ShaderNodeNewGeometry")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Location"].default_value = offset
    mapping.inputs["Scale"].default_value = (1.0 / scale_x, 1.0 / scale_y, 1.0 / scale_z)
    links.new(geometry.outputs["Position"], mapping.inputs["Vector"])
    return mapping.outputs["Vector"]


def texture(setup, path, colorspace=SRGB):
    """An image node for the material being set up, with its own texture coordinates. `setup` is
    (node tree, function that makes the coordinates, projection)."""
    tree, uv, projection = setup
    node = image_node(tree.nodes, path, colorspace, projection)
    tree.links.new(uv(tree.nodes, tree.links), node.inputs["Vector"])
    return node


def link_normal_map(setup, path, bsdf):
    tree = setup[0]
    normal_image = texture(setup, path, NON_COLOR)
    normal = tree.nodes.new("ShaderNodeNormalMap")
    tree.links.new(normal_image.outputs["Color"], normal.inputs["Color"])
    tree.links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])


def link_arm_map(setup, path, bsdf):
    """AO / roughness / metallic packed in R / G / B."""
    tree = setup[0]
    arm = texture(setup, path, NON_COLOR)
    split = tree.nodes.new("ShaderNodeSeparateColor")
    tree.links.new(arm.outputs["Color"], split.inputs["Color"])
    tree.links.new(split.outputs["Green"], bsdf.inputs["Roughness"])
    tree.links.new(split.outputs["Blue"], bsdf.inputs["Metallic"])


def link_plain_maps(setup, maps, bsdf):
    """The maps that go straight into one input of the shader."""
    for key, colorspace, socket in PLAIN_MAPS:
        if key in maps:
            setup[0].links.new(texture(setup, maps[key], colorspace).outputs["Color"], bsdf.inputs[socket])


def pbr_material(name, maps, uv, projection=FLAT):
    """maps: dict with color / normal / roughness / arm / ao / metal / emission paths."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    setup = (mat.node_tree, uv, projection)
    bsdf = mat.node_tree.nodes[PRINCIPLED]
    mat.node_tree.links.new(texture(setup, maps["color"]).outputs["Color"], bsdf.inputs["Base Color"])
    if "normal" in maps:
        link_normal_map(setup, maps["normal"], bsdf)
    if "arm" in maps:
        link_arm_map(setup, maps["arm"], bsdf)
    link_plain_maps(setup, maps, bsdf)
    return mat


def ambientcg_maps(asset):
    folder = os.path.join(TEXTURES, asset)
    prefix = os.path.join(folder, f"{asset}_2K-JPG_")
    maps = {"color": prefix + "Color.jpg", "normal": prefix + "NormalGL.jpg", "roughness": prefix + "Roughness.jpg"}
    for key, suffix in (("metal", "Metalness.jpg"), ("emission", "Emission.jpg")):
        if os.path.exists(prefix + suffix):
            maps[key] = prefix + suffix
    return maps


def polyhaven_maps(asset):
    prefix = os.path.join(TEXTURES, asset, f"{asset}_")
    return {"color": prefix + "diff_2k.jpg", "normal": prefix + "nor_gl_2k.jpg", "arm": prefix + "arm_2k.jpg"}


def wet_roughness(tree, bsdf):
    """The road's own roughness, lowered everywhere."""
    rough_source = bsdf.inputs["Roughness"].links[0].from_socket
    wet = tree.nodes.new("ShaderNodeMath")
    wet.operation = "MULTIPLY"
    wet.inputs[1].default_value = WET_ROUGHNESS_FACTOR
    tree.links.new(rough_source, wet.inputs[0])
    return wet.outputs["Value"]


def puddle_mask(tree):
    """Puddles: a large noise pattern decides where the road is a near-mirror (0 dry, 1 puddle)."""
    pattern = tree.nodes.new("ShaderNodeTexNoise")
    pattern.inputs["Scale"].default_value = PUDDLE_NOISE_SCALE
    tree.links.new(tree.nodes.new("ShaderNodeNewGeometry").outputs["Position"], pattern.inputs["Vector"])
    ramp = tree.nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value = PUDDLE_THRESHOLD - PUDDLE_SOFTNESS
    ramp.inputs["From Max"].default_value = PUDDLE_THRESHOLD + PUDDLE_SOFTNESS
    tree.links.new(pattern.outputs["Fac"], ramp.inputs["Value"])
    return ramp.outputs["Result"]


def mirror_puddles(tree, bsdf, wet, puddles):
    mix_rough = tree.nodes.new("ShaderNodeMix")
    mix_rough.data_type = "FLOAT"
    tree.links.new(puddles, mix_rough.inputs["Factor"])
    tree.links.new(wet, mix_rough.inputs["A"])
    mix_rough.inputs["B"].default_value = PUDDLE_ROUGHNESS
    tree.links.new(mix_rough.outputs["Result"], bsdf.inputs["Roughness"])


def darken_puddles(tree, bsdf, puddles):
    """Wet surfaces are darker."""
    color_source = bsdf.inputs["Base Color"].links[0].from_socket
    darken = tree.nodes.new("ShaderNodeMix")
    darken.data_type = "RGBA"
    darken.blend_type = "MULTIPLY"
    tree.links.new(puddles, darken.inputs["Factor"])
    tree.links.new(color_source, darken.inputs["A"])
    darken.inputs["B"].default_value = (PUDDLE_DARKEN, PUDDLE_DARKEN, PUDDLE_DARKEN, 1.0)
    tree.links.new(darken.outputs["Result"], bsdf.inputs["Base Color"])


def wet_road_material():
    road_uv = functools.partial(world_mapping, scale=ROAD_TILING, offset=(ROAD_WIDTH * HALF, 0.0, 0.0))
    mat = pbr_material("WetRoad", ambientcg_maps(ROAD_TEXTURE), road_uv)
    tree = mat.node_tree
    bsdf = tree.nodes[PRINCIPLED]
    wet = wet_roughness(tree, bsdf)
    puddles = puddle_mask(tree)
    mirror_puddles(tree, bsdf, wet, puddles)
    darken_puddles(tree, bsdf, puddles)
    return mat


def facade_material(asset, tile, glow):
    uv = functools.partial(world_mapping, scale=(tile, tile, tile))
    mat = pbr_material(f"{asset}_glow{glow:.1f}", ambientcg_maps(asset), uv, projection=BOX)
    if "Emission Strength" in mat.node_tree.nodes[PRINCIPLED].inputs:
        mat.node_tree.nodes[PRINCIPLED].inputs["Emission Strength"].default_value = glow
    return mat


def concrete_material(name):
    return pbr_material(name, polyhaven_maps(CONCRETE_TEXTURE), functools.partial(world_mapping, scale=CURB_TILING),
                        projection=BOX)


# --- Geometry -----------------------------------------------------------------------------------

def span(a, b):
    """(lower, upper) of two coordinates given in either order."""
    return min(a, b), max(a, b)


def slab(name, x_range, y_range, z_range, mat):
    """A box given by where it starts and ends along each axis."""
    ranges = (x_range, y_range, z_range)
    size = tuple(upper - lower for lower, upper in ranges)
    center = tuple((lower + upper) * HALF for lower, upper in ranges)
    obj = kit.box(name, center, size, bake_scale=True)
    obj.data.materials.append(mat)
    return obj


def build_street():
    length = (STREET_START, STREET_END)
    half = ROAD_WIDTH * HALF
    slab(ROAD_NAME, (-half, half), length, (ROAD_UNDERSIDE, 0.0), wet_road_material())
    sidewalk = pbr_material("Sidewalk", polyhaven_maps(SIDEWALK_TEXTURE),
                            functools.partial(world_mapping, scale=SIDEWALK_TILING))
    curb = concrete_material("Curb")
    for side in SIDES:
        inner, curb_outer, walk_outer = half * side, CURB_LINE * side, FACADE_LINE * side
        slab(f"Curb{side:+d}", span(inner, curb_outer), length, (ROAD_UNDERSIDE, CURB_HEIGHT), curb)
        slab(f"Sidewalk{side:+d}", span(curb_outer, walk_outer), length, (ROAD_UNDERSIDE, CURB_HEIGHT), sidewalk)


def building_look(rng):
    """(is it a glass tower?, height, facade material) of the next building."""
    if rng.random() < TOWER_CHANCE:
        height = rng.uniform(TOWER_MIN_HEIGHT, TOWER_MAX_HEIGHT)
        return True, height, facade_material(GLASS_FACADE, GLASS_FACADE_TILE, 0.0)
    height = rng.uniform(BUILDING_MIN_HEIGHT, BUILDING_MAX_HEIGHT)
    glow_range = (WINDOW_GLOW_MIN, WINDOW_GLOW_MAX)
    return False, height, facade_material(rng.choice(BRICK_FACADES), BRICK_FACADE_TILE, rng.uniform(*glow_range))


def build_building(name, lot, height, materials):
    """The block of a building with its plinth and cornice. `lot` is (side of the street, y where
    it starts, width along the street); `materials` is (facade, plinth and cornice)."""
    side, y, width = lot
    facade, concrete = materials
    x_near, x_far = FACADE_LINE * side, (FACADE_LINE + BUILDING_DEPTH) * side
    y_end = y + width - BUILDING_GAP
    slab(name, span(x_near, x_far), (y, y_end), (CURB_HEIGHT, height), facade)
    plinth_near = x_near - PLINTH_PROTRUSION * side
    slab(name + "_plinth", span(plinth_near, x_far), (y, y_end), (CURB_HEIGHT, CURB_HEIGHT + PLINTH_HEIGHT), concrete)
    overhang_near = x_near - CORNICE_OVERHANG * side
    slab(name + "_cornice", span(overhang_near, x_far), (y - CORNICE_OVERHANG, y_end + CORNICE_OVERHANG),
         (height, height + CORNICE_HEIGHT), concrete)


def build_buildings(rng):
    """Returns the fronts of the brick buildings, which get shops."""
    concrete = concrete_material("Cornice")
    shop_fronts = []
    for side in SIDES:
        y = STREET_START
        index = 0
        while y < STREET_END:
            width = rng.uniform(BUILDING_MIN_WIDTH, BUILDING_MAX_WIDTH)
            tower, height, facade = building_look(rng)
            name = f"{BUILDING_PREFIX}{side:+d}_{index}"
            build_building(name, (side, y, width), height, (facade, concrete))
            if not tower:
                shop_fronts.append(Front(name, side, FACADE_LINE * side, y, width - BUILDING_GAP))
            y += width
            index += 1
    return shop_fronts


def glow_material(name, color, strength):
    mat = kit.material(name, color)
    bsdf = mat.node_tree.nodes[PRINCIPLED]
    bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
    bsdf.inputs["Emission Strength"].default_value = strength
    return mat


def wall_box(front, part, offset, size, mat):
    """A box in front of a facade, named after it and `part`. `offset` is (meters from the wall to
    the box's center, y, z); `size` is (depth out of the wall, width along the street, height)."""
    out, y, z = offset
    return kit.box(f"{front.name}_{part}", (front.x_near - front.side * out, y, z), size, mat=mat, bake_scale=True)


def build_shop_band(front, rng):
    """The painted band across the whole ground floor."""
    band_color = rng.randrange(len(SHOP_BAND_COLORS))
    band = kit.material(f"ShopBand{band_color}", SHOP_BAND_COLORS[band_color], roughness=SHOP_BAND_ROUGHNESS)
    center = front.start + front.width * HALF
    wall_box(front, "band", (SHOP_BAND_DEPTH * HALF, center, (SHOP_FLOOR + SHOP_BAND_TOP) * HALF),
             (SHOP_BAND_DEPTH, front.width, SHOP_BAND_TOP - SHOP_FLOOR), band)


def shop_bays(front):
    """The building's front cut into equal bays: as many as fit."""
    bays = max(1, int(front.width // SHOP_BAY_WIDTH))
    bay = front.width / bays
    return [Bay(f"{front.name}_shop{index}", front.side, front.x_near, front.start + bay * (index + HALF), bay)
            for index in range(bays)]


def build_shop_fronts(shop_fronts, rng):
    frame = kit.material("ShopFrame", SHOP_FRAME_COLOR, roughness=SHOP_FRAME_ROUGHNESS, metallic=SHOP_FRAME_METALLIC)
    dark_glass = kit.material("ShopDarkGlass", SHOP_DARK_GLASS, roughness=SHOP_GLASS_ROUGHNESS)
    for front in shop_fronts:
        build_shop_band(front, rng)
        for bay in shop_bays(front):
            build_shop_bay(bay, frame, dark_glass, rng)


def window_width(bay):
    return bay.width - SHOP_BAY_MARGIN


def build_display(bay, rng):
    dark = kit.material("ShopDisplay", DISPLAY_COLOR, roughness=DISPLAY_ROUGHNESS)
    width = window_width(bay)
    wall_box(bay, "counter", (DISPLAY_OUT, bay.center, SHOP_WINDOW_BOTTOM + DISPLAY_COUNTER_HEIGHT * HALF),
             (DISPLAY_DEPTH, width, DISPLAY_COUNTER_HEIGHT), dark)
    for item in range(rng.randint(*DISPLAY_ITEMS)):
        item_width = rng.uniform(*DISPLAY_ITEM_WIDTH)
        height = rng.uniform(*DISPLAY_ITEM_HEIGHT)
        y = bay.center + rng.uniform(-HALF, HALF) * (width - item_width)
        wall_box(bay, f"item{item}", (DISPLAY_OUT, y, SHOP_WINDOW_BOTTOM + DISPLAY_COUNTER_HEIGHT + height * HALF),
                 (DISPLAY_DEPTH, item_width, height), dark)


def shop_glass(rng):
    """The glowing glass of an open shop."""
    glow = rng.randrange(len(SHOP_GLOWS))
    strength = round(rng.uniform(*SHOP_GLOW_STRENGTH) * GLOW_STEPS) / GLOW_STEPS  # few distinct materials
    return glow_material(f"ShopGlow{glow}_{strength}", SHOP_GLOWS[glow], strength)


def build_window_frame(bay, frame):
    """Vertical bars across the window, and a bar along its top and its sill."""
    width = window_width(bay)
    mullions = max(MIN_MULLIONS, round(width / SHOP_MULLION_SPACING) + 1)
    for bar in range(mullions):
        y = bay.center - width * HALF + width * bar / (mullions - 1)
        wall_box(bay, f"bar{bar}", (FRAME_OUT, y, SHOP_WINDOW_Z),
                 (SHOP_FRAME, SHOP_FRAME, SHOP_WINDOW_HEIGHT + SHOP_FRAME), frame)
    for edge, z in SHOP_WINDOW_EDGES:
        wall_box(bay, edge, (FRAME_OUT, bay.center, z), (SHOP_FRAME, width, SHOP_FRAME), frame)


def build_awning(bay, rng):
    color = rng.randrange(len(AWNING_COLORS))
    cloth = kit.material(f"Awning{color}", AWNING_COLORS[color], roughness=AWNING_ROUGHNESS)
    slope = math.radians(AWNING_SLOPE_DEGREES)
    drop = math.sin(slope) * AWNING_REACH * HALF
    awning = wall_box(bay, "awning", (AWNING_REACH * HALF, bay.center, AWNING_HEIGHT - drop),
                      (AWNING_REACH, bay.width - AWNING_SIDE_MARGIN * BOTH_SIDES, AWNING_THICKNESS), cloth)
    awning.rotation_euler = (0.0, -bay.side * slope, 0.0)  # lower toward the road


def build_neon(bay, rng):
    color = rng.randrange(len(NEON_COLORS))
    depth, share, height = NEON_SIZE
    wall_box(bay, "neon", (SHOP_BAND_DEPTH + depth * HALF, bay.center, NEON_HEIGHT),
             (depth, bay.width * share, height), glow_material(f"Neon{color}", NEON_COLORS[color], NEON_STRENGTH))


def on_sidewalk(bay, from_wall, y):
    """A spot on the sidewalk, `from_wall` meters in front of the bay's wall."""
    return (bay.x_near - bay.side * from_wall, y, CURB_HEIGHT)


def furnish_sidewalk(bay, lit, rng):
    """Cafe tables in front of some open shops, a bench in front of some closed ones, a plant."""
    if lit and rng.random() < CAFE_CHANCE:
        for offset in CAFE_TABLE_OFFSETS:
            instance(CAFE_SET, f"{bay.name}_cafe{offset:+.0f}", on_sidewalk(bay, CAFE_FROM_WALL, bay.center + offset))
    elif not lit and rng.random() < BENCH_CHANCE:
        instance(BENCH, f"{bay.name}_bench", on_sidewalk(bay, BENCH_FROM_WALL, bay.center), -QUARTER_TURN * bay.side)
    if rng.random() < PLANT_CHANCE:
        beside_window = bay.center + window_width(bay) * HALF + SHOP_BAY_MARGIN * HALF
        instance(POTTED_PLANT, f"{bay.name}_plant", on_sidewalk(bay, PLANT_FROM_WALL, beside_window),
                 rng.uniform(0.0, FULL_TURN_DEGREES))


def build_shop_bay(bay, frame, dark_glass, rng):
    lit = rng.random() < SHOP_LIT_CHANCE
    glass = shop_glass(rng) if lit else dark_glass
    wall_box(bay, "window", (WINDOW_OUT, bay.center, SHOP_WINDOW_Z),
             (SHOP_WINDOW_DEPTH, window_width(bay), SHOP_WINDOW_HEIGHT), glass)
    if lit:
        build_display(bay, rng)
    build_window_frame(bay, frame)
    if rng.random() < AWNING_CHANCE:
        build_awning(bay, rng)
    if lit and rng.random() < NEON_CHANCE:
        build_neon(bay, rng)
    furnish_sidewalk(bay, lit, rng)


# --- Scanned models ------------------------------------------------------------------------------

_templates = {}
_instances = {}  # model -> the roots of its copies in the scene


def thin(objects, ratio):
    """Replaces the meshes of `objects` by ones with that share of their triangles."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        if obj.type != MESH:
            continue
        obj.modifiers.new("Decimate", DECIMATE).ratio = ratio
        depsgraph.update()
        heavy = obj.data
        obj.data = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
        obj.modifiers.clear()
        bpy.data.meshes.remove(heavy)


def template(asset):
    """Imports a Poly Haven model once; returns its root objects (kept out of the scene)."""
    if asset in _templates:
        return _templates[asset]
    path = glob.glob(os.path.join(MODELS, asset, "*.gltf"))[0]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    created = [obj for obj in bpy.data.objects if obj not in before]
    if asset in IMPORT_DECIMATE:
        thin(created, IMPORT_DECIMATE[asset])
    for obj in created:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
    _templates[asset] = created
    return created


def instance(asset, name, location, rotation_degrees=0.0, scale=1.0):
    """Linked copy of a whole model (meshes are shared, so heavy scans cost no extra memory)."""
    root = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(root)
    _instances.setdefault(asset, []).append(root)
    copies = {}
    for obj in template(asset):
        copy = obj.copy()
        bpy.context.collection.objects.link(copy)
        copies[obj] = copy
    for original, copy in copies.items():
        copy.parent = copies.get(original.parent, root)
        if original.parent is None:
            copy.matrix_parent_inverse.identity()
    root.location = location
    root.rotation_euler = (0.0, 0.0, math.radians(rotation_degrees))
    root.scale = (scale, scale, scale)
    return root, list(copies.values())


def is_lamp_glass(mat):
    return mat is not None and any(word in mat.name.lower() for word in LAMP_GLASS_WORDS)


def light_up_lamp(parts):
    for obj in parts:
        for slot in getattr(obj, "material_slots", []):
            if not is_lamp_glass(slot.material):
                continue
            bsdf = slot.material.node_tree.nodes.get(PRINCIPLED)
            if bsdf is not None:
                bsdf.inputs["Emission Color"].default_value = (*LAMP_COLOR_TEMPERATURE, 1.0)
                bsdf.inputs["Emission Strength"].default_value = BULB_EMISSION


def lamp_rows():
    """(index, y) of each pair of street lamps, down the street."""
    y = STREET_START
    index = 0
    while y < STREET_END:
        yield index, y
        y += LAMP_SPACING
        index += 1


def build_lamp(side, index, y):
    """A lamp post and the light it casts."""
    x = (CURB_LINE + LAMP_FROM_CURB) * side
    _, parts = instance(LAMP_MODEL, f"Lamp{side:+d}_{index}", (x, y, CURB_HEIGHT),
                        QUARTER_TURN if side < 0 else -QUARTER_TURN)
    light_up_lamp(parts)
    bpy.ops.object.light_add(type="POINT", location=(x - LAMP_ARM_REACH * side, y, LAMP_HEAD_HEIGHT))
    light = bpy.context.active_object
    light.name = f"{LAMP_LIGHT_PREFIX}{side:+d}_{index}"
    light.data.energy = LAMP_LIGHT_WATTS
    light.data.color = LAMP_COLOR_TEMPERATURE
    light.data.shadow_soft_size = LAMP_LIGHT_RADIUS


def build_tree(rng, side, index, y):
    """The tree after a lamp: every few of them a jacaranda."""
    turn, spread = rng.uniform(0.0, FULL_TURN_DEGREES), rng.uniform(*TREE_SCALE_SPREAD)
    jacaranda = (index + (side > 0)) % JACARANDA_EVERY == 1
    asset, scale = (JACARANDA, JACARANDA_SCALE) if jacaranda else (STREET_TREE, TREE_SCALE)
    location = ((CURB_LINE + TREE_FROM_CURB) * side, y + TREE_OFFSET, CURB_HEIGHT)
    instance(asset, f"Tree{side:+d}_{index}", location, turn, scale * spread)


def build_props():
    for index, (asset, side, y, rotation) in enumerate(PROPS):
        instance(asset, f"Prop_{asset}_{index}", ((CURB_LINE + PROP_FROM_CURB) * side, y, CURB_HEIGHT), rotation)
    for index, (x, y) in enumerate(MANHOLES + MORE_MANHOLES):
        instance(MANHOLE, f"Manhole{index}", (x, y, 0.0))


def build_street_furniture(rng):
    for index, y in lamp_rows():
        for side in SIDES:
            build_lamp(side, index, y)
            build_tree(rng, side, index, y)
    build_props()
    scatter_along_curbs()


def place_scatter(name, asset, spot, rotation):
    """One thing at the curb, or the few that make a pile of trash."""
    if asset != TRASH_PILE_SLOT:
        instance(asset, name, (*spot, CURB_HEIGHT), rotation)
        return
    for part, (model, (dx, dy), turn) in enumerate(TRASH_PILE):
        instance(model, f"{name}_{part}", (spot[0] + dx, spot[1] + dy, CURB_HEIGHT), turn)


def scatter_along_curbs():
    rng = random.Random(SEED + SCATTER_SEED_OFFSET)
    choices = [entry for entry in SCATTER for _ in range(entry[1])]
    slots = itertools.product(lamp_rows(), SIDES, enumerate(SCATTER_SLOTS))
    for (index, y), side, (slot, offset) in slots:
        asset, _, rotation = rng.choice(choices)
        spot = ((CURB_LINE + PROP_FROM_CURB) * side, y + offset)
        if asset is not None and spot[1] >= SCATTER_FROM:
            place_scatter(f"Scatter{side:+d}_{index}_{slot}", asset, spot, rotation * side)


# --- Sky, camera, render --------------------------------------------------------------------------

def brightest_column(image):
    """Horizontal position (0..1) of the brightest pixel of an image."""
    width, height = image.size
    pixels = image.pixels[:]
    best, best_u = -1.0, 0.0
    for row in range(height):
        for column in range(width):
            i = (row * width + column) * RGBA_CHANNELS
            luminance = sum(pixels[i:i + RGB_CHANNELS])
            if luminance > best:
                best, best_u = luminance, (column + HALF) / width
    return best_u


def hdri_sun_azimuth(path):
    """Azimuth (radians, Blender world) of the brightest spot of an equirectangular HDRI."""
    image = bpy.data.images.load(path, check_existing=True)
    probe = image.copy()
    probe.scale(HDRI_ANALYSIS_WIDTH, HDRI_ANALYSIS_WIDTH // EQUIRECTANGULAR_ASPECT)
    best_u = brightest_column(probe)
    bpy.data.images.remove(probe)
    # Blender equirect: u = 0.5 - azimuth / (2 pi)
    return (HALF - best_u) * math.tau


def build_world():
    world = bpy.data.worlds.new("Sunset")
    bpy.context.scene.world = world
    world.use_nodes = True
    nodes, links = world.node_tree.nodes, world.node_tree.links
    background = nodes["Background"]
    background.inputs["Strength"].default_value = WORLD_STRENGTH
    env = nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(HDRI, check_existing=True)
    mapping = nodes.new("ShaderNodeMapping")
    rotation = hdri_sun_azimuth(HDRI) - SUN_TARGET_AZIMUTH
    mapping.inputs["Rotation"].default_value = (0.0, 0.0, rotation)
    links.new(nodes.new("ShaderNodeTexCoord").outputs["Generated"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], env.inputs["Vector"])
    links.new(env.outputs["Color"], background.inputs["Color"])


def build_haze():
    """Thin atmospheric haze in a box around the street (a world volume would hide the sky)."""
    half_width = FACADE_LINE + BUILDING_DEPTH
    mat = bpy.data.materials.new(HAZE_NAME)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.remove(nodes[PRINCIPLED])
    volume = nodes.new("ShaderNodeVolumePrincipled")
    volume.inputs["Density"].default_value = HAZE_DENSITY
    volume.inputs["Anisotropy"].default_value = HAZE_ANISOTROPY
    links.new(volume.outputs["Volume"], nodes["Material Output"].inputs["Volume"])
    haze = slab(HAZE_NAME, (-half_width, half_width), (STREET_START, STREET_END), (0.0, HAZE_HEIGHT), mat)
    haze.visible_shadow = False


def root_name(collection_name):
    return collection_name + " (placed)"


def strip_custom_properties(objects):
    """Asset libraries and add-ons leave their own custom properties on objects (thousands of
    them on the car); the engine would load each as a game property."""
    for obj in objects:
        for key in list(obj.keys()):
            del obj[key]


def place_asset(folder, collection_name, location, heading_degrees):
    """Appends a collection from .deps/assets/ad/<folder>/<folder>.blend under a new root empty
    placed at `location`. Returns the root, or None when the asset is not downloaded."""
    blend = os.path.join(CAST_DIR, folder, folder + ".blend")
    if not os.path.exists(blend):
        print(f"[showcase] {folder} not downloaded (python3 -I tools/assets/fetch_ad_assets.py)")
        return None
    bpy.ops.wm.append(directory=blend + "/Collection/", filename=collection_name)
    collection = bpy.data.collections[collection_name]
    strip_custom_properties(collection.all_objects)
    root = bpy.data.objects.new(root_name(collection_name), None)
    bpy.context.collection.objects.link(root)
    for obj in collection.all_objects:
        if obj.parent is None:
            obj.parent = root
    root.location = location
    root.rotation_euler = (0.0, 0.0, math.radians(heading_degrees))
    return root


def build_cast():
    for folder, collection, location, heading in CARS + PEOPLE:
        place_asset(folder, collection, location, heading)


def add_fill_light(target):
    location = Vector(CAMERA_LOCATION) + Vector(FILL_LIGHT_OFFSET)
    data = bpy.data.lights.new("FillLight", type="AREA")
    data.energy = FILL_LIGHT_WATTS
    data.size = FILL_LIGHT_SIZE
    data.color = FILL_LIGHT_COLOR
    light = bpy.data.objects.new("FillLight", data)
    bpy.context.collection.objects.link(light)
    light.location = location
    light.rotation_euler = kit.look_rotation(location, target)
    light.visible_camera = False
    light.visible_glossy = False  # no studio softbox reflected in the paint


def focus_on_car(camera, hero):
    """Focus on the hero car's nose."""
    focus = bpy.data.objects.new("FocusTarget", None)
    bpy.context.collection.objects.link(focus)
    focus.location = hero.location + Vector((0.0, 0.0, CAR_FOCUS_HEIGHT))
    camera.data.dof.focus_object = focus
    add_fill_light(focus.location)


def build_camera():
    camera = kit.camera("ShowcaseCamera", CAMERA_LOCATION, CAMERA_TARGET)
    camera.data.lens_unit = "MILLIMETERS"
    camera.data.lens = LENS_MM
    camera.data.sensor_width = SENSOR_MM
    camera.data.dof.use_dof = True
    camera.data.dof.aperture_fstop = F_STOP
    hero = bpy.data.objects.get(root_name(HERO_CAR))
    if SHOT in SHOTS_WITH_CAR and hero is not None:
        focus_on_car(camera, hero)
    else:
        camera.data.dof.focus_distance = FOCUS_DISTANCE
    return camera


def use_gpu(scene):
    scene.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = GPU_BACKEND
    prefs.get_devices()
    for device in prefs.devices:
        device.use = device.type == GPU_BACKEND
    scene.cycles.device = "GPU"


def set_render_quality(scene):
    scene.cycles.samples = SAMPLES
    scene.cycles.adaptive_threshold = NOISE_THRESHOLD
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = GPU_BACKEND
    scene.cycles.max_bounces = MAX_BOUNCES
    scene.cycles.use_auto_tile = True
    scene.cycles.tile_size = TILE_SIZE


def set_render_output(scene, output_path):
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    scene.render.resolution_percentage = FULL_RESOLUTION
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = LOOK
    scene.view_settings.exposure = EXPOSURE
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = output_path
    # Blender otherwise writes the .blend's path, date and more into the image's metadata.
    for name in dir(scene.render):
        if name.startswith("use_stamp"):
            setattr(scene.render, name, False)


def configure_render(output_path):
    scene = bpy.context.scene
    use_gpu(scene)
    set_render_quality(scene)
    set_render_output(scene, output_path)


# --- Real-time export ---------------------------------------------------------------------------

def is_world_mapping(node):
    return node.type == "MAPPING" and any(link.from_node.type == "NEW_GEOMETRY" for link in node.inputs["Vector"].links)


def world_mapping_node(material):
    """The Mapping node of a material built by world_mapping() (fed by world positions), or None.
    Imported models also use Mapping nodes, but on their own UVs: those must be left alone."""
    if material is None or not material.use_nodes:
        return None
    return next((node for node in material.node_tree.nodes if is_world_mapping(node)), None)


def main_axis(normal):
    """Index of the axis a direction mostly points along."""
    return max(range(AXES), key=lambda axis: abs(normal[axis]))


def face_uv(mapped, axis, box):
    """UV of a mapped point: seen from above, or (box projection) along the face's main axis."""
    if not box or axis == Z_AXIS:
        return (mapped.x, mapped.y)
    if axis == X_AXIS:
        return (mapped.y, mapped.z)
    return (mapped.x, mapped.z)


def bake_world_uvs(obj):
    """Turns a material's world-space mapping (meters) into a real UV map for glTF."""
    material = obj.active_material
    mapping = world_mapping_node(material)
    if mapping is None or obj.type != MESH:
        return
    location = Vector(mapping.inputs["Location"].default_value)
    scale = Vector(mapping.inputs["Scale"].default_value)
    box = any(n.type == "TEX_IMAGE" and n.projection == BOX for n in material.node_tree.nodes)
    mesh = obj.data
    layer = mesh.uv_layers.active or mesh.uv_layers.new(name="UVMap")
    world = obj.matrix_world
    rotation = world.to_3x3()
    for polygon in mesh.polygons:
        axis = main_axis(rotation @ polygon.normal)
        for loop_index in polygon.loop_indices:
            p = world @ mesh.vertices[mesh.loops[loop_index].vertex_index].co
            m = Vector((p.x * scale.x + location.x, p.y * scale.y + location.y, p.z * scale.z + location.z))
            layer.data[loop_index].uv = face_uv(m, axis, box)


def simplify_material(material):
    """Image textures read the UV map directly (glTF cannot express world mapping or puddles)."""
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = nodes.get(PRINCIPLED)
    for node in nodes:
        if node.type != "TEX_IMAGE":
            continue
        node.projection = FLAT
        for link in list(node.inputs["Vector"].links):
            links.remove(link)
        name = node.image.name.lower() if node.image else ""
        if bsdf is not None and "color" in name and "emission" not in name:
            links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
        if bsdf is not None and "roughness" in name:
            links.new(node.outputs["Color"], bsdf.inputs["Roughness"])


def replace_by_lighter(mesh, ratio, depsgraph):
    """Every object using `mesh` gets, instead, one copy of it with that share of its triangles."""
    probe = bpy.data.objects.new("DecimateProbe", mesh)
    bpy.context.collection.objects.link(probe)
    modifier = probe.modifiers.new("Decimate", DECIMATE)
    modifier.ratio = ratio
    depsgraph.update()
    light = bpy.data.meshes.new_from_object(probe.evaluated_get(depsgraph))
    for obj in bpy.data.objects:
        if obj.data == mesh and obj is not probe:
            obj.data = light
    bpy.data.objects.remove(probe)


def decimate_scans():
    """Replaces the shared meshes of the heavy scanned models by lighter copies, still shared by
    every copy of the model."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for asset, ratio in ENGINE_DECIMATE.items():
        meshes = {child.data for root in _instances.get(asset, []) for child in root.children_recursive
                  if child.type == MESH}
        for mesh in meshes:
            replace_by_lighter(mesh, ratio, depsgraph)


def cap_textures():
    for image in bpy.data.images:
        width, height = image.size
        longest = max(width, height)
        if image.is_float or longest <= ENGINE_MAX_TEXTURE:  # the HDRI sky is not exported
            continue
        factor = ENGINE_MAX_TEXTURE / longest
        image.scale(max(1, round(width * factor)), max(1, round(height * factor)))


def triangle_count(obj, depsgraph):
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    count = sum(len(polygon.vertices) - (TRIANGLE_CORNERS - 1) for polygon in mesh.polygons)
    evaluated.to_mesh_clear()
    return count


def set_subdivision(obj, levels):
    for modifier in obj.modifiers:
        if modifier.type == SUBSURF:
            modifier.levels = modifier.render_levels = levels


def fit_triangle_budget(root, budget):
    """Decimates the meshes under `root` so that together they stay within `budget` triangles."""
    meshes = [obj for obj in root.children_recursive if obj.type == MESH and not obj.hide_render]
    for obj in meshes:
        set_subdivision(obj, ENGINE_SUBSURF_LEVELS)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()
    counts = {obj: triangle_count(obj, depsgraph) for obj in meshes}
    total = sum(counts.values())
    if total <= budget:
        return
    ratio = budget / total
    for obj, count in counts.items():
        if count >= ENGINE_DECIMATE_MIN_TRIANGLES:
            obj.modifiers.new("EngineDecimate", DECIMATE).ratio = ratio
    print(f"[showcase] {root.name}: {total} -> ~{int(total * ratio)} triangles for the engine")


def by_prefix(table, name):
    """The value of the first entry of `table` whose key `name` starts with, or None."""
    return next((value for prefix, value in table.items() if name.startswith(prefix)), None)


def plain_bsdf(material):
    """Replaces the material's nodes by a lone Principled BSDF, and returns it."""
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    nodes.clear()
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    links.new(bsdf.outputs["BSDF"], nodes.new("ShaderNodeOutputMaterial").inputs["Surface"])
    return bsdf


def flatten_car_materials():
    """Replaces the car's procedural materials by a plain Principled BSDF the engine can show."""
    for material in bpy.data.materials:
        values = by_prefix(ENGINE_CAR_MATERIALS, material.name)
        if values is None:
            continue
        color, metallic, roughness = values
        bsdf = plain_bsdf(material)
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness


def light_up_car_lamps():
    for material in bpy.data.materials:
        glow = by_prefix(ENGINE_CAR_GLOW, material.name)
        if glow is None:
            continue
        color, strength = glow
        bsdf = plain_bsdf(material)
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = strength


def add_car_lights(car):
    for name, location, color, watts in ENGINE_CAR_LIGHTS:
        light = kit.point_light(name, ORIGIN, color, watts)
        light.parent = car
        light.location = location


def drop_vertex_colors(obj):
    """Scans may carry painted vertex colors, which engines multiply into the base color."""
    while obj.type == MESH and obj.data.color_attributes:
        obj.data.color_attributes.remove(obj.data.color_attributes[0])


def is_albedo(node):
    return node.type == "TEX_IMAGE" and node.image and ALBEDO_WORD in node.image.name.lower()


def use_albedo_texture(material):
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    albedo = next((n for n in nodes if is_albedo(n)), None)
    if bsdf is None or albedo is None:
        return
    links.new(albedo.outputs["Color"], bsdf.inputs["Base Color"])
    # The scans derive a bump from the photo; as a glTF normal map that makes them look chromed.
    for link in list(bsdf.inputs["Normal"].links):
        links.remove(link)
    bsdf.inputs["Roughness"].default_value = ENGINE_PERSON_ROUGHNESS


def use_albedo_textures(obj):
    """The scans mix several maps into their skin; glTF wants the albedo straight into Base Color
    (otherwise the exporter picks the subsurface map and people come out almost black)."""
    for slot in obj.material_slots:
        material = slot.material
        if material is None or not material.use_nodes:
            continue
        drop_vertex_colors(obj)
        use_albedo_texture(material)


def meshes_under(root):
    return [obj for obj in root.children_recursive if obj.type == MESH]


def bake_modifiers(root):
    """Turns the modifier stacks of everything under `root` into plain meshes (copies can then
    share them, and they can be skinned) and drops the helper objects hidden from renders."""
    for obj in meshes_under(root):
        if obj.hide_render:
            bpy.data.objects.remove(obj)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()
    for obj in meshes_under(root):
        if not obj.modifiers:
            continue
        baked = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph), depsgraph=depsgraph)
        obj.modifiers.clear()
        obj.data = baked


def has_several_materials(obj):
    return len({slot.material for slot in obj.material_slots if slot.material}) > 1


def split_by_material(car):
    """One object per material, so painting a car (the `tint` property colors a whole object)
    does not also color the glass or trim that shared a mesh with a painted panel."""
    mixed = [obj for obj in meshes_under(car) if has_several_materials(obj)]
    if not mixed:
        return
    bpy.ops.object.select_all(action="DESELECT")
    for obj in mixed:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mixed[0]
    bpy.ops.mesh.separate(type="MATERIAL")
    bpy.ops.object.select_all(action="DESELECT")


def world_center(obj):
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return sum(corners, Vector()) / len(corners)


def turns_with_wheel(obj, offset, radius):
    """Whether a part sits at a wheel. `offset` is from the wheel's center to the part's, in car
    space: x sideways, y along, z up."""
    if abs(offset.x) > ENGINE_WHEEL_HALF_WIDTH or Vector((offset.y, offset.z)).length > radius * ENGINE_WHEEL_REACH:
        return False
    # Larger than the wheel: a body panel that happens to be centered near it.
    return max(obj.dimensions) <= radius * BOTH_SIDES * ENGINE_WHEEL_REACH


def add_wheel_pivot(car, index, center):
    pivot = bpy.data.objects.new(f"{car.name} Wheel {index}", None)
    bpy.context.collection.objects.link(pivot)
    pivot.parent = car
    pivot.location = car.matrix_world.inverted() @ center
    bpy.context.view_layer.update()
    return pivot


def adopt(parent, child):
    """Parents `child` to `parent` without moving it."""
    world = child.matrix_world.copy()
    child.parent = parent
    child.matrix_parent_inverse = parent.matrix_world.inverted() @ world @ child.matrix_basis.inverted()


def group_wheels(car):
    """Parents the parts of each wheel (tire, rim, brake disc) to a pivot at its axle, named
    "... Wheel N", so scripts can spin them."""
    bpy.context.view_layer.update()
    tires = [obj for obj in meshes_under(car) if obj.name.startswith(CAR_TIRE_PREFIX)]
    parts = [obj for obj in meshes_under(car) if CAR_FIXED_PART_WORD not in obj.name.lower()]
    for index, tire in enumerate(tires):
        center = world_center(tire)
        radius = max(tire.dimensions) * HALF
        pivot = add_wheel_pivot(car, index, center)
        to_car = car.matrix_world.inverted().to_3x3()
        for obj in parts:
            if turns_with_wheel(obj, to_car @ (world_center(obj) - center), radius):
                adopt(pivot, obj)


def paint_car(car, color):
    for obj in meshes_under(car):
        if any(slot.material and slot.material.name.startswith(ENGINE_PAINT_PREFIXES) for slot in obj.material_slots):
            obj[TINT] = color


def copy_car(car, name):
    """A copy of the car sharing all its meshes (lights are not copied)."""
    copies = {}
    for obj in [car] + list(car.children_recursive):
        if obj.type == "LIGHT":
            continue
        copy = obj.copy()
        bpy.context.collection.objects.link(copy)
        copies[obj] = copy
    for original, copy in copies.items():
        if original.parent in copies:
            copy.parent = copies[original.parent]
            copy.matrix_parent_inverse = original.matrix_parent_inverse.copy()
    copies[car].name = name
    return copies[car]


def add_traffic(model):
    for name, x, y, heading, color in ENGINE_TRAFFIC:
        other = copy_car(model, name)
        other.location = (x, y, model.location.z)
        other.rotation_euler = (0.0, 0.0, math.radians(heading))
        paint_car(other, color)
        if name.startswith(PARKED_PREFIX):
            other["tag"] = PARKED_TAG


def lighter_car(car, name, keep):
    """A copy of the car without its subdivision, for when it is seen from further away: a quarter
    of the triangles, or less with `keep` below 1. (Made before the modifiers are baked: the copy
    shares the meshes, not the stack.)"""
    light = copy_car(car, name)
    for obj in meshes_under(light):
        obj.data = obj.data.copy()  # its own meshes: each version is cut up separately
        set_subdivision(obj, 0)
        if keep < 1.0 and len(obj.data.polygons) >= ENGINE_DECIMATE_MIN_TRIANGLES:
            obj.modifiers.new("FarDecimate", DECIMATE).ratio = keep
    return light


def merge_level(car, level, visible_from, visible_to):
    """Moves the parts of `level` (another version of the same car) under `car`, drawn only between
    those distances from the camera."""
    for obj in meshes_under(level):
        obj[VISIBLE_FROM] = visible_from
        if visible_to is not None:
            obj[VISIBLE_TO] = visible_to
    bpy.context.view_layer.update()
    for child in list(level.children):
        adopt(car, child)
    bpy.data.objects.remove(level)


def join_parts(parts):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        if obj.data.users > 1:
            obj.data = obj.data.copy()  # joining writes into the mesh: not into one others share
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()


def join_by_material(objects):
    """Joins the mesh objects that share a parent and a material: what the engine pays for is the
    number of things it draws, far more than their triangles."""
    groups = {}
    for obj in objects:
        material = obj.material_slots[0].material if obj.material_slots else None
        groups.setdefault((obj.parent, material), []).append(obj)
    for parts in groups.values():
        if len(parts) >= MIN_TO_JOIN:
            join_parts(parts)
    bpy.ops.object.select_all(action="DESELECT")


def prepare_car(car):
    bake_modifiers(car)
    split_by_material(car)
    group_wheels(car)
    join_by_material(meshes_under(car))  # a few dozen objects per car instead of hundreds


def smooth_shading(car):
    """Without subdivision the panels are few, large polygons: shade them as one curved surface."""
    for obj in meshes_under(car):
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
        sharp = obj.data.attributes.get("sharp_edge")
        if sharp is not None:
            obj.data.attributes.remove(sharp)


def build_car_levels(car):
    """Three versions of the car, each drawn at its own range of distances. Every car in the
    street is a copy of this one, so they all share the three sets of meshes."""
    near_range, mid_range = ENGINE_CAR_DETAIL_RANGES
    middle = lighter_car(car, "CarMiddle", 1.0)
    far = lighter_car(car, "CarFar", ENGINE_CAR_FAR_KEEP)
    for version in (car, middle, far):
        prepare_car(version)
    smooth_shading(middle)
    smooth_shading(far)
    for obj in meshes_under(car):
        obj[VISIBLE_TO] = near_range
    merge_level(car, middle, near_range, mid_range)
    merge_level(car, far, mid_range, None)


def lighten_car():
    car = bpy.data.objects.get(root_name(HERO_CAR))
    if car is None:
        return
    fit_triangle_budget(car, ENGINE_CAR_TRIANGLES)
    build_car_levels(car)
    add_traffic(car)
    paint_car(car, ENGINE_HERO_PAINT)
    add_car_lights(car)


def make_walker(root, name):
    """Along the sidewalk, on legs that really walk."""
    heading, x = ENGINE_WALKERS[name]
    root.rotation_euler = (0.0, 0.0, math.radians(heading))
    root.location.x = x
    bodies = meshes_under(root)
    if len(bodies) != 1:
        raise SystemExit(f"{name}: expected one mesh to rig, found {len(bodies)}")
    _, speed = walk_rig.rig_walker(bodies[0], name)
    root["speed"] = speed  # a fact of the rig: the pace at which its feet do not slide


def lighten_person(name):
    root = bpy.data.objects.get(root_name(name))
    if root is None:
        return
    fit_triangle_budget(root, ENGINE_PERSON_TRIANGLES)
    bake_modifiers(root)
    for obj in root.children_recursive:
        use_albedo_textures(obj)
    if name in ENGINE_WALKERS:
        make_walker(root, name)


def lighten_cast():
    cap_textures()
    flatten_car_materials()
    light_up_car_lamps()
    lighten_car()
    for person in PEOPLE:
        lighten_person(person[1])


def unit(value):
    """Noise comes as -1..1; this brings it to 0..1."""
    return value * HALF + HALF


def worn_height(x, y):
    """Height of the worn road at (x, y): a crown, broad waves and a few dips, never above 0."""
    wave = noise.noise(Vector((x / ENGINE_ROAD_WAVE_SIZE, y / ENGINE_ROAD_WAVE_SIZE, 0.0)))  # -1..1
    dip = unit(noise.noise(Vector((x / ENGINE_ROAD_DIP_SIZE, y / ENGINE_ROAD_DIP_SIZE, NOISE_LAYER_DIPS))))
    dip = max(dip - ENGINE_ROAD_DIP_THRESHOLD, 0.0) / (1.0 - ENGINE_ROAD_DIP_THRESHOLD)
    crown = ENGINE_ROAD_CROWN * (abs(x) / (ROAD_WIDTH * HALF)) ** ENGINE_ROAD_CROWN_POWER
    return -crown - ENGINE_ROAD_WAVE_DEPTH * unit(wave) - ENGINE_ROAD_DIP_DEPTH * dip


def sink_potholes(x, y, z):
    """The height `z` of the road at (x, y), lowered where a pothole is."""
    for hole_x, hole_y, radius, depth in ENGINE_POTHOLES:
        ragged = radius * (1.0 + ENGINE_POTHOLE_EDGE_NOISE * noise.noise(
            Vector((x / ENGINE_POTHOLE_EDGE_SCALE, y / ENGINE_POTHOLE_EDGE_SCALE, NOISE_LAYER_POTHOLES))))
        inside = 1.0 - math.hypot(x - hole_x, y - hole_y) / ragged
        if inside > 0.0:
            z -= depth * min(inside * ENGINE_POTHOLE_WALL_STEEPNESS, 1.0)  # steep walls, flat bottom
    return z


def road_vertices():
    vertices = []
    for row in range(ENGINE_ROAD_ROWS + 1):
        y = STREET_START + STREET_LENGTH * row / ENGINE_ROAD_ROWS
        for column in range(ENGINE_ROAD_COLUMNS + 1):
            x = -ROAD_WIDTH * HALF + ROAD_WIDTH * column / ENGINE_ROAD_COLUMNS
            vertices.append((x, y, sink_potholes(x, y, worn_height(x, y))))
    return vertices


def road_faces():
    """One quad per grid cell, as indices into road_vertices()."""
    faces = []
    for row in range(ENGINE_ROAD_ROWS):
        for column in range(ENGINE_ROAD_COLUMNS):
            a = row * (ENGINE_ROAD_COLUMNS + 1) + column
            above = a + ENGINE_ROAD_COLUMNS + 1
            faces.append((a, a + 1, above + 1, above))
    return faces


def mark_potholes():
    for index, (x, y, radius, depth) in enumerate(ENGINE_POTHOLES):
        kit.empty(f"Pothole{index + 1}", (x, y, 0.0), props={"tag": POTHOLE_TAG, "radius": radius, "depth": depth})


def deform_road():
    """Replaces the flat road by a worn one: a grid with a crown, broad waves and a few dips."""
    old = bpy.data.objects.get(ROAD_NAME)
    if old is None:
        return
    material = old.active_material
    bpy.data.objects.remove(old)
    mesh = bpy.data.meshes.new(ROAD_NAME)
    mesh.from_pydata(road_vertices(), [], road_faces())
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    road = bpy.data.objects.new(ROAD_NAME, mesh)
    bpy.context.collection.objects.link(road)
    road.data.materials.append(material)
    mark_potholes()


def add_leaves():
    """Leaf templates: hidden prefabs the game copies (see falling_leaves.rb)."""
    mesh = bpy.data.meshes.new("Leaf")
    mesh.from_pydata(LEAF_POINTS, [], LEAF_FACES)
    material = kit.material("Leaf", LEAF_BASE_COLOR, roughness=LEAF_ROUGHNESS)
    material.use_backface_culling = False  # seen from both sides
    mesh.materials.append(material)
    for index, color in enumerate(LEAF_COLORS):
        leaf = bpy.data.objects.new(f"Leaf{index + 1}", mesh)
        bpy.context.collection.objects.link(leaf)
        leaf["prefab"] = True
        leaf[TINT] = color


def join_building_parts():
    """Each building's boxes (plinth, cornice, shop band, windows, frames, awnings) become one
    object per material."""
    buildings = [obj for obj in bpy.data.objects
                 if obj.name.startswith(BUILDING_PREFIX) and obj.name.count("_") == 1]
    for building in buildings:
        prefix = building.name + "_"
        join_by_material([obj for obj in bpy.data.objects
                          if obj.type == MESH and obj.name.startswith(prefix) and obj.parent is None])


def dress_for_engine():
    """Sunset mood with what the engine has: an environment object, a warm sun, brighter street
    lamps."""
    environment = bpy.data.objects.new("Environment", None)
    bpy.context.collection.objects.link(environment)
    for key, value in ENGINE_ENVIRONMENT.items():
        environment[key] = value
    for obj in bpy.data.objects:
        if obj.type == "LIGHT" and obj.name.startswith(LAMP_LIGHT_PREFIX):
            obj.data.energy = ENGINE_LAMP_WATTS


def bake_street_uvs():
    for obj in bpy.context.scene.objects:
        bake_world_uvs(obj)
    for material in bpy.data.materials:
        if world_mapping_node(material) is not None:
            simplify_material(material)


def add_engine_sun():
    """The sky's sun becomes a real light (the engine does not light with HDRIs)."""
    sun_location = (math.cos(SUN_TARGET_AZIMUTH) * ENGINE_SUN_DISTANCE,
                    math.sin(SUN_TARGET_AZIMUTH) * ENGINE_SUN_DISTANCE, ENGINE_SUN_HEIGHT)
    kit.sun("Sun", sun_location, ORIGIN, ENGINE_SUN_STRENGTH).data.color = ENGINE_SUN_COLOR


def simplify_for_engine():
    haze = bpy.data.objects.get(HAZE_NAME)
    if haze is not None:
        bpy.data.objects.remove(haze)
    deform_road()  # before the UV bake below, which reads the road's vertices
    bake_street_uvs()
    decimate_scans()
    lighten_cast()
    join_building_parts()


def export_for_engine():
    if bpy.data.objects.get(root_name(HERO_CAR)) is None:
        # The game (and its test) is about driving the car.
        raise SystemExit("the car is missing: run python3 -I tools/assets/fetch_ad_assets.py before exporting")
    simplify_for_engine()
    add_engine_sun()
    dress_for_engine()
    add_leaves()
    car = bpy.data.objects[root_name(HERO_CAR)]
    car.rotation_euler = (0.0, 0.0, math.radians(ENGINE_CAR_HEADING))
    car.location.x = ENGINE_LANE
    os.makedirs(GAME_DIR, exist_ok=True)
    kit.export_glb(os.path.join(GAME_DIR, "scene.glb"))


def build_scene():
    rng = random.Random(SEED)
    build_street()
    shop_fronts = build_buildings(rng)
    build_street_furniture(rng)
    build_shop_fronts(shop_fronts, random.Random(SEED + SHOP_SEED_OFFSET))
    build_world()
    build_haze()
    build_cast()
    build_camera()


def main():
    out_dir = kit.output_path_from_argv("showcase")
    for needed in (MODELS, TEXTURES, HDRI):
        if not os.path.exists(needed):
            raise SystemExit(f"{needed} is missing: run python3 -I tools/assets/fetch_showcase.py first")
    kit.reset_scene()
    build_scene()
    renders = os.path.join(out_dir, "renders")
    os.makedirs(renders, exist_ok=True)
    configure_render(os.path.join(renders, f"{SHOT}_preview.png" if PREVIEW else f"{SHOT}.png"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "street.blend"))
    if RENDER:
        bpy.ops.render.render(write_still=True)
        print(f"[showcase] rendered {bpy.context.scene.render.filepath}")
    if EXPORT:
        export_for_engine()  # last: it simplifies the scene in memory (the saved .blend is untouched)


main()
