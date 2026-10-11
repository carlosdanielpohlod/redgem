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

import glob
import math
import os
import random
import sys

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
    "bloom": 0.25,
    "vignette": 0.3,
    "sun_glow": 0.7,
    "wind": 0.7,
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

# Things along the curb, down the whole street: (model, weight, rotation on the right-hand sidewalk;
# the left-hand one mirrors it). None is an empty spot.
SCATTER_SEED_OFFSET = 2
SCATTER_FROM = 50.0                # beyond the hand-placed props near the camera
SCATTER_SLOTS = (4.5, 13.5)        # meters after each lamp (the tree is at 9)
SCATTER = [
    (None, 3, 0.0), ("metal_trash_can", 3, 90.0), ("fire_hydrant", 2, 90.0), ("utility_box_01", 1, -90.0),
    ("utility_box_02", 1, -90.0), ("modular_street_seating", 2, -90.0), ("planter_box_01", 2, 90.0),
    ("planter_box_02", 2, 90.0), ("planter_box_03", 2, 90.0), ("trash_pile", 2, 0.0),
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
    ("melissa_white", "Standing Cute Girl White Jacket Jeans", (7.9, 66.0, SIDEWALK_TOP), -70.0),  # wide trousers: she stands
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


# --- Materials ---------------------------------------------------------------------------------

def image_node(nodes, path, colorspace="sRGB", projection="FLAT"):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(path, check_existing=True)
    node.image.colorspace_settings.name = colorspace
    node.projection = projection
    if projection == "BOX":
        node.projection_blend = BOX_PROJECTION_BLEND
    return node


def world_mapping(nodes, links, scale_x, scale_y, scale_z=1.0, offset=(0.0, 0.0, 0.0)):
    """Texture coordinates from world position, so tiling is in meters on any object."""
    geometry = nodes.new("ShaderNodeNewGeometry")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Location"].default_value = offset
    mapping.inputs["Scale"].default_value = (1.0 / scale_x, 1.0 / scale_y, 1.0 / scale_z)
    links.new(geometry.outputs["Position"], mapping.inputs["Vector"])
    return mapping.outputs["Vector"]


def pbr_material(name, maps, uv, projection="FLAT"):
    """maps: dict with color / normal / roughness / arm / ao / metal / emission paths."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    color = image_node(nodes, maps["color"], projection=projection)
    links.new(uv(nodes, links), color.inputs["Vector"])
    links.new(color.outputs["Color"], bsdf.inputs["Base Color"])
    if "normal" in maps:
        normal_image = image_node(nodes, maps["normal"], "Non-Color", projection)
        links.new(uv(nodes, links), normal_image.inputs["Vector"])
        normal = nodes.new("ShaderNodeNormalMap")
        links.new(normal_image.outputs["Color"], normal.inputs["Color"])
        links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
    if "arm" in maps:  # AO / roughness / metallic packed in R / G / B
        arm = image_node(nodes, maps["arm"], "Non-Color", projection)
        links.new(uv(nodes, links), arm.inputs["Vector"])
        split = nodes.new("ShaderNodeSeparateColor")
        links.new(arm.outputs["Color"], split.inputs["Color"])
        links.new(split.outputs["Green"], bsdf.inputs["Roughness"])
        links.new(split.outputs["Blue"], bsdf.inputs["Metallic"])
    if "roughness" in maps:
        rough = image_node(nodes, maps["roughness"], "Non-Color", projection)
        links.new(uv(nodes, links), rough.inputs["Vector"])
        links.new(rough.outputs["Color"], bsdf.inputs["Roughness"])
    if "metal" in maps:
        metal = image_node(nodes, maps["metal"], "Non-Color", projection)
        links.new(uv(nodes, links), metal.inputs["Vector"])
        links.new(metal.outputs["Color"], bsdf.inputs["Metallic"])
    if "emission" in maps:
        emission = image_node(nodes, maps["emission"], projection=projection)
        links.new(uv(nodes, links), emission.inputs["Vector"])
        links.new(emission.outputs["Color"], bsdf.inputs["Emission Color"])
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


def wet_road_material():
    road_uv = lambda nodes, links: world_mapping(nodes, links, ROAD_WIDTH, ROAD_TILE_LENGTH,  # noqa: E731
                                                 offset=(ROAD_WIDTH / 2, 0.0, 0.0))
    mat = pbr_material("WetRoad", ambientcg_maps("Road007"), road_uv)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    rough_source = bsdf.inputs["Roughness"].links[0].from_socket
    wet = nodes.new("ShaderNodeMath")
    wet.operation = "MULTIPLY"
    wet.inputs[1].default_value = WET_ROUGHNESS_FACTOR
    links.new(rough_source, wet.inputs[0])
    # Puddles: a large noise pattern decides where the road is a near-mirror.
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = PUDDLE_NOISE_SCALE
    links.new(nodes.new("ShaderNodeNewGeometry").outputs["Position"], noise.inputs["Vector"])
    ramp = nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value = PUDDLE_THRESHOLD - PUDDLE_SOFTNESS
    ramp.inputs["From Max"].default_value = PUDDLE_THRESHOLD + PUDDLE_SOFTNESS
    links.new(noise.outputs["Fac"], ramp.inputs["Value"])
    mix_rough = nodes.new("ShaderNodeMix")
    mix_rough.data_type = "FLOAT"
    links.new(ramp.outputs["Result"], mix_rough.inputs["Factor"])
    links.new(wet.outputs["Value"], mix_rough.inputs["A"])
    mix_rough.inputs["B"].default_value = PUDDLE_ROUGHNESS
    links.new(mix_rough.outputs["Result"], bsdf.inputs["Roughness"])
    # Wet surfaces are darker.
    color_source = bsdf.inputs["Base Color"].links[0].from_socket
    darken = nodes.new("ShaderNodeMix")
    darken.data_type = "RGBA"
    darken.blend_type = "MULTIPLY"
    links.new(ramp.outputs["Result"], darken.inputs["Factor"])
    links.new(color_source, darken.inputs["A"])
    darken.inputs["B"].default_value = (PUDDLE_DARKEN, PUDDLE_DARKEN, PUDDLE_DARKEN, 1.0)
    links.new(darken.outputs["Result"], bsdf.inputs["Base Color"])
    return mat


def facade_material(asset, tile, glow):
    uv = lambda nodes, links: world_mapping(nodes, links, tile, tile, tile)  # noqa: E731
    mat = pbr_material(f"{asset}_glow{glow:.1f}", ambientcg_maps(asset), uv, projection="BOX")
    if "Emission Strength" in mat.node_tree.nodes["Principled BSDF"].inputs:
        mat.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = glow
    return mat


# --- Geometry -----------------------------------------------------------------------------------

def slab(name, x_min, x_max, y_min, y_max, z_min, z_max, mat):
    size = (x_max - x_min, y_max - y_min, z_max - z_min)
    center = ((x_min + x_max) / 2, (y_min + y_max) / 2, (z_min + z_max) / 2)
    obj = kit.box(name, center, size, bake_scale=True)
    obj.data.materials.append(mat)
    return obj


def build_street():
    y0, y1 = STREET_START, STREET_START + STREET_LENGTH
    half = ROAD_WIDTH / 2
    slab("Road", -half, half, y0, y1, ROAD_UNDERSIDE, 0.0, wet_road_material())
    sidewalk = pbr_material("Sidewalk", polyhaven_maps("concrete_pavement"),
                            lambda n, l: world_mapping(n, l, SIDEWALK_TILE, SIDEWALK_TILE))
    curb = pbr_material("Curb", polyhaven_maps("concrete_floor_worn_001"),
                        lambda n, l: world_mapping(n, l, CURB_TILE, CURB_TILE, CURB_TILE), projection="BOX")
    for side in (-1, 1):
        inner = half * side
        curb_outer = (half + CURB_WIDTH) * side
        walk_outer = (half + CURB_WIDTH + SIDEWALK_WIDTH) * side
        slab(f"Curb{side:+d}", min(inner, curb_outer), max(inner, curb_outer), y0, y1, ROAD_UNDERSIDE, CURB_HEIGHT, curb)
        slab(f"Sidewalk{side:+d}", min(curb_outer, walk_outer), max(curb_outer, walk_outer), y0, y1, -0.1,
             CURB_HEIGHT, sidewalk)


def build_buildings(rng):
    brick = ["Facade018A", "Facade020A"]
    concrete = pbr_material("Cornice", polyhaven_maps("concrete_floor_worn_001"),
                            lambda n, l: world_mapping(n, l, CURB_TILE, CURB_TILE, CURB_TILE), projection="BOX")
    front = ROAD_WIDTH / 2 + CURB_WIDTH + SIDEWALK_WIDTH
    shop_fronts = []
    for side in (-1, 1):
        y = STREET_START
        index = 0
        while y < STREET_START + STREET_LENGTH:
            width = rng.uniform(BUILDING_MIN_WIDTH, BUILDING_MAX_WIDTH)
            tower = rng.random() < TOWER_CHANCE
            if tower:
                height = rng.uniform(TOWER_MIN_HEIGHT, TOWER_MAX_HEIGHT)
                mat = facade_material("Facade006", GLASS_FACADE_TILE, 0.0)
            else:
                height = rng.uniform(BUILDING_MIN_HEIGHT, BUILDING_MAX_HEIGHT)
                mat = facade_material(rng.choice(brick), BRICK_FACADE_TILE,
                                      rng.uniform(WINDOW_GLOW_MIN, WINDOW_GLOW_MAX))
            x_near, x_far = front * side, (front + BUILDING_DEPTH) * side
            name = f"Building{side:+d}_{index}"
            slab(name, min(x_near, x_far), max(x_near, x_far), y, y + width - BUILDING_GAP, CURB_HEIGHT, height, mat)
            plinth_near = x_near - PLINTH_PROTRUSION * side
            slab(name + "_plinth", min(plinth_near, x_far), max(plinth_near, x_far), y, y + width - BUILDING_GAP,
                 CURB_HEIGHT, CURB_HEIGHT + PLINTH_HEIGHT, concrete)
            overhang_near = x_near - CORNICE_OVERHANG * side
            slab(name + "_cornice", min(overhang_near, x_far), max(overhang_near, x_far), y - CORNICE_OVERHANG,
                 y + width - BUILDING_GAP + CORNICE_OVERHANG, height, height + CORNICE_HEIGHT, concrete)
            if not tower:
                shop_fronts.append((name, side, x_near, y, width - BUILDING_GAP))
            y += width
            index += 1
    return shop_fronts


def glow_material(name, color, strength):
    mat = kit.material(name, color)
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
    bsdf.inputs["Emission Strength"].default_value = strength
    return mat


def wall_box(name, side, x_near, out, y, z, size, mat):
    """A box in front of a facade: `out` meters from the wall to its center, `size` is (depth out
    of the wall, width along the street, height)."""
    return kit.box(name, (x_near - side * out, y, z), size, mat=mat, bake_scale=True)


def build_shop_fronts(shop_fronts, rng):
    frame = kit.material("ShopFrame", SHOP_FRAME_COLOR, roughness=0.4, metallic=0.6)
    dark_glass = kit.material("ShopDarkGlass", SHOP_DARK_GLASS, roughness=SHOP_GLASS_ROUGHNESS)
    floor = CURB_HEIGHT + PLINTH_HEIGHT
    for name, side, x_near, y_start, width in shop_fronts:
        band_color = rng.randrange(len(SHOP_BAND_COLORS))
        band = kit.material(f"ShopBand{band_color}", SHOP_BAND_COLORS[band_color], roughness=0.6)
        wall_box(f"{name}_band", side, x_near, SHOP_BAND_DEPTH / 2, y_start + width / 2, (floor + SHOP_BAND_TOP) / 2,
                 (SHOP_BAND_DEPTH, width, SHOP_BAND_TOP - floor), band)
        bays = max(1, int(width // SHOP_BAY_WIDTH))
        bay = width / bays
        for index in range(bays):
            center = y_start + bay * (index + 0.5)
            build_shop_bay(f"{name}_shop{index}", side, x_near, center, bay, floor, frame, dark_glass, rng)


def build_display(name, side, x_near, center, width, sill, rng):
    dark = kit.material("ShopDisplay", DISPLAY_COLOR, roughness=0.9)
    out = SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH + DISPLAY_DEPTH / 2
    wall_box(f"{name}_counter", side, x_near, out, center, sill + DISPLAY_COUNTER_HEIGHT / 2,
             (DISPLAY_DEPTH, width, DISPLAY_COUNTER_HEIGHT), dark)
    for item in range(rng.randint(*DISPLAY_ITEMS)):
        item_width = rng.uniform(*DISPLAY_ITEM_WIDTH)
        height = rng.uniform(*DISPLAY_ITEM_HEIGHT)
        y = center + rng.uniform(-0.5, 0.5) * (width - item_width)
        wall_box(f"{name}_item{item}", side, x_near, out, y, sill + DISPLAY_COUNTER_HEIGHT + height / 2,
                 (DISPLAY_DEPTH, item_width, height), dark)


def build_shop_bay(name, side, x_near, center, bay, floor, frame, dark_glass, rng):
    window_width = bay - SHOP_BAY_MARGIN
    window_z = floor + SHOP_WINDOW_SILL + SHOP_WINDOW_HEIGHT / 2
    lit = rng.random() < SHOP_LIT_CHANCE
    if lit:
        glow = rng.randrange(len(SHOP_GLOWS))
        strength = round(rng.uniform(*SHOP_GLOW_STRENGTH) * 2) / 2  # half steps: few distinct materials
        glass = glow_material(f"ShopGlow{glow}_{strength}", SHOP_GLOWS[glow], strength)
    else:
        glass = dark_glass
    wall_box(f"{name}_window", side, x_near, SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH / 2, center, window_z,
             (SHOP_WINDOW_DEPTH, window_width, SHOP_WINDOW_HEIGHT), glass)
    if lit:
        build_display(name, side, x_near, center, window_width, window_z - SHOP_WINDOW_HEIGHT / 2, rng)
    frame_out = SHOP_BAND_DEPTH + SHOP_WINDOW_DEPTH + SHOP_FRAME / 2
    mullions = max(2, round(window_width / SHOP_MULLION_SPACING) + 1)
    for bar in range(mullions):
        y = center - window_width / 2 + window_width * bar / (mullions - 1)
        wall_box(f"{name}_bar{bar}", side, x_near, frame_out, y, window_z,
                 (SHOP_FRAME, SHOP_FRAME, SHOP_WINDOW_HEIGHT + SHOP_FRAME), frame)
    for edge, z in (("top", window_z + SHOP_WINDOW_HEIGHT / 2), ("sill", window_z - SHOP_WINDOW_HEIGHT / 2)):
        wall_box(f"{name}_{edge}", side, x_near, frame_out, center, z, (SHOP_FRAME, window_width, SHOP_FRAME), frame)
    if rng.random() < AWNING_CHANCE:
        color = rng.randrange(len(AWNING_COLORS))
        cloth = kit.material(f"Awning{color}", AWNING_COLORS[color], roughness=AWNING_ROUGHNESS)
        drop = math.sin(math.radians(AWNING_SLOPE_DEGREES)) * AWNING_REACH / 2
        awning = wall_box(f"{name}_awning", side, x_near, AWNING_REACH / 2, center, AWNING_HEIGHT - drop,
                          (AWNING_REACH, bay - AWNING_SIDE_MARGIN * 2, AWNING_THICKNESS), cloth)
        awning.rotation_euler = (0.0, -side * math.radians(AWNING_SLOPE_DEGREES), 0.0)  # lower toward the road
    if lit and rng.random() < NEON_CHANCE:
        color = rng.randrange(len(NEON_COLORS))
        depth, share, height = NEON_SIZE
        wall_box(f"{name}_neon", side, x_near, SHOP_BAND_DEPTH + depth / 2, center, NEON_HEIGHT,
                 (depth, bay * share, height), glow_material(f"Neon{color}", NEON_COLORS[color], NEON_STRENGTH))
    if lit and rng.random() < CAFE_CHANCE:
        for offset in (-CAFE_TABLE_SPACING / 2, CAFE_TABLE_SPACING / 2):
            instance("outdoor_table_chair_set_01", f"{name}_cafe{offset:+.0f}",
                     (x_near - side * CAFE_FROM_WALL, center + offset, CURB_HEIGHT))
    elif not lit and rng.random() < BENCH_CHANCE:
        instance("painted_wooden_bench", f"{name}_bench", (x_near - side * BENCH_FROM_WALL, center, CURB_HEIGHT),
                 -90.0 * side)
    if rng.random() < PLANT_CHANCE:
        instance("potted_plant_02", f"{name}_plant",
                 (x_near - side * PLANT_FROM_WALL, center + window_width / 2 + SHOP_BAY_MARGIN / 2, CURB_HEIGHT),
                 rng.uniform(0.0, FULL_TURN_DEGREES))


# --- Scanned models ------------------------------------------------------------------------------

_templates = {}
_instances = {}  # model -> the roots of its copies in the scene


def template(asset):
    """Imports a Poly Haven model once; returns its root objects (kept out of the scene)."""
    if asset in _templates:
        return _templates[asset]
    path = glob.glob(os.path.join(MODELS, asset, "*.gltf"))[0]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    created = [obj for obj in bpy.data.objects if obj not in before]
    if asset in IMPORT_DECIMATE:
        depsgraph = bpy.context.evaluated_depsgraph_get()
        for obj in created:
            if obj.type != "MESH":
                continue
            obj.modifiers.new("Decimate", "DECIMATE").ratio = IMPORT_DECIMATE[asset]
            depsgraph.update()
            heavy = obj.data
            obj.data = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
            obj.modifiers.clear()
            bpy.data.meshes.remove(heavy)
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


def light_up_lamp(parts):
    for obj in parts:
        for slot in getattr(obj, "material_slots", []):
            mat = slot.material
            if mat is None or not any(word in mat.name.lower() for word in ("glass", "bulb", "light")):
                continue
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf is not None:
                bsdf.inputs["Emission Color"].default_value = (*LAMP_COLOR_TEMPERATURE, 1.0)
                bsdf.inputs["Emission Strength"].default_value = BULB_EMISSION


def build_street_furniture(rng):
    curb_line = ROAD_WIDTH / 2 + CURB_WIDTH
    y = STREET_START
    index = 0
    while y < STREET_START + STREET_LENGTH:
        for side in (-1, 1):
            x = (curb_line + LAMP_FROM_CURB) * side
            _, parts = instance("street_lamp_01", f"Lamp{side:+d}_{index}", (x, y, CURB_HEIGHT),
                                90.0 if side < 0 else -90.0)
            light_up_lamp(parts)
            bpy.ops.object.light_add(type="POINT", location=(x - LAMP_ARM_REACH * side, y, LAMP_HEAD_HEIGHT))
            light = bpy.context.active_object
            light.name = f"LampLight{side:+d}_{index}"
            light.data.energy = LAMP_LIGHT_WATTS
            light.data.color = LAMP_COLOR_TEMPERATURE
            light.data.shadow_soft_size = LAMP_LIGHT_RADIUS
            tree_x = (curb_line + TREE_FROM_CURB) * side
            turn, spread = rng.uniform(0.0, FULL_TURN_DEGREES), rng.uniform(*TREE_SCALE_SPREAD)
            if (index + (side > 0)) % JACARANDA_EVERY == 1:
                instance("jacaranda_tree", f"Tree{side:+d}_{index}", (tree_x, y + TREE_OFFSET, CURB_HEIGHT), turn,
                         JACARANDA_SCALE * spread)
            else:
                instance("island_tree_02", f"Tree{side:+d}_{index}", (tree_x, y + TREE_OFFSET, CURB_HEIGHT), turn,
                         TREE_SCALE * spread)
        y += LAMP_SPACING
        index += 1
    for index, (asset, side, y_prop, rotation) in enumerate(PROPS):
        x = (curb_line + PROP_FROM_CURB) * side
        instance(asset, f"Prop_{asset}_{index}", (x, y_prop, CURB_HEIGHT), rotation)
    for index, (x, y_cover) in enumerate(MANHOLES + MORE_MANHOLES):
        instance("water_manhole_cover", f"Manhole{index}", (x, y_cover, 0.0))
    scatter_along_curbs(curb_line)


def scatter_along_curbs(curb_line):
    rng = random.Random(SEED + SCATTER_SEED_OFFSET)
    choices = [entry for entry in SCATTER for _ in range(entry[1])]
    y = STREET_START
    index = 0
    while y < STREET_START + STREET_LENGTH:
        for side in (-1, 1):
            for slot, offset in enumerate(SCATTER_SLOTS):
                asset, _, rotation = rng.choice(choices)
                place = ((curb_line + PROP_FROM_CURB) * side, y + offset)
                if asset is None or place[1] < SCATTER_FROM:
                    continue
                name = f"Scatter{side:+d}_{index}_{slot}"
                if asset == "trash_pile":
                    for part, (model, (dx, dy), turn) in enumerate(TRASH_PILE):
                        instance(model, f"{name}_{part}", (place[0] + dx, place[1] + dy, CURB_HEIGHT), turn)
                else:
                    instance(asset, name, (*place, CURB_HEIGHT), rotation * side)
        y += LAMP_SPACING
        index += 1


# --- Sky, camera, render --------------------------------------------------------------------------

def hdri_sun_azimuth(path):
    """Azimuth (radians, Blender world) of the brightest spot of an equirectangular HDRI."""
    image = bpy.data.images.load(path, check_existing=True)
    probe = image.copy()
    probe.scale(HDRI_ANALYSIS_WIDTH, HDRI_ANALYSIS_WIDTH // 2)
    width, height = probe.size
    pixels = probe.pixels[:]
    best, best_u = -1.0, 0.0
    channels = RGBA_CHANNELS
    for row in range(height):
        for column in range(width):
            i = (row * width + column) * channels
            luminance = pixels[i] + pixels[i + 1] + pixels[i + 2]
            if luminance > best:
                best, best_u = luminance, (column + 0.5) / width
    bpy.data.images.remove(probe)
    # Blender equirect: u = 0.5 - azimuth / (2 pi)
    return (0.5 - best_u) * 2.0 * math.pi


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
    half_width = ROAD_WIDTH / 2 + CURB_WIDTH + SIDEWALK_WIDTH + BUILDING_DEPTH
    mat = bpy.data.materials.new("Haze")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.remove(nodes["Principled BSDF"])
    volume = nodes.new("ShaderNodeVolumePrincipled")
    volume.inputs["Density"].default_value = HAZE_DENSITY
    volume.inputs["Anisotropy"].default_value = HAZE_ANISOTROPY
    links.new(volume.outputs["Volume"], nodes["Material Output"].inputs["Volume"])
    haze = slab("Haze", -half_width, half_width, STREET_START, STREET_START + STREET_LENGTH, 0.0, HAZE_HEIGHT, mat)
    haze.visible_shadow = False


def root_name(collection_name):
    return collection_name + " (placed)"


def place_asset(folder, collection_name, location, heading_degrees):
    """Appends a collection from .deps/assets/ad/<folder>/<folder>.blend under a new root empty
    placed at `location`. Returns the root, or None when the asset is not downloaded."""
    blend = os.path.join(CAST_DIR, folder, folder + ".blend")
    if not os.path.exists(blend):
        print(f"[showcase] {folder} not downloaded (python3 -I tools/assets/fetch_ad_assets.py)")
        return None
    bpy.ops.wm.append(directory=blend + "/Collection/", filename=collection_name)
    collection = bpy.data.collections[collection_name]
    # Asset libraries and add-ons leave their own custom properties on objects (thousands of
    # them on the car); the engine would load each as a game property.
    for obj in collection.all_objects:
        for key in list(obj.keys()):
            del obj[key]
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


def build_camera():
    camera = kit.camera("ShowcaseCamera", CAMERA_LOCATION, CAMERA_TARGET)
    camera.data.lens_unit = "MILLIMETERS"
    camera.data.lens = LENS_MM
    camera.data.sensor_width = SENSOR_MM
    camera.data.dof.use_dof = True
    camera.data.dof.aperture_fstop = F_STOP
    hero = bpy.data.objects.get(root_name(HERO_CAR))
    if SHOT in ("hero", "ad") and hero is not None:
        # Focus on the hero car's nose.
        focus = bpy.data.objects.new("FocusTarget", None)
        bpy.context.collection.objects.link(focus)
        focus.location = hero.location + Vector((0.0, 0.0, CAR_FOCUS_HEIGHT))
        camera.data.dof.focus_object = focus
        add_fill_light(focus.location)
    else:
        camera.data.dof.focus_distance = FOCUS_DISTANCE
    return camera


def configure_render(output_path):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = "OPTIX"
    prefs.get_devices()
    for device in prefs.devices:
        device.use = device.type == "OPTIX"
    scene.cycles.device = "GPU"
    scene.cycles.samples = SAMPLES
    scene.cycles.adaptive_threshold = NOISE_THRESHOLD
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = "OPTIX"
    scene.cycles.max_bounces = MAX_BOUNCES
    scene.cycles.use_auto_tile = True
    scene.cycles.tile_size = TILE_SIZE
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = LOOK
    scene.view_settings.exposure = EXPOSURE
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = output_path
    # Blender otherwise writes the .blend's path, date and more into the image's metadata.
    for name in dir(scene.render):
        if name.startswith("use_stamp"):
            setattr(scene.render, name, False)


# --- Real-time export ---------------------------------------------------------------------------

def world_mapping_node(material):
    """The Mapping node of a material built by world_mapping() (fed by world positions), or None.
    Imported models also use Mapping nodes, but on their own UVs: those must be left alone."""
    if material is None or not material.use_nodes:
        return None
    for node in material.node_tree.nodes:
        if node.type == "MAPPING" and any(link.from_node.type == "NEW_GEOMETRY" for link in node.inputs["Vector"].links):
            return node
    return None


def bake_world_uvs(obj):
    """Turns a material's world-space mapping (meters) into a real UV map for glTF."""
    material = obj.active_material
    mapping = world_mapping_node(material)
    if mapping is None or obj.type != "MESH":
        return
    location = Vector(mapping.inputs["Location"].default_value)
    scale = Vector(mapping.inputs["Scale"].default_value)
    box = any(n.type == "TEX_IMAGE" and n.projection == "BOX" for n in material.node_tree.nodes)
    mesh = obj.data
    layer = mesh.uv_layers.active or mesh.uv_layers.new(name="UVMap")
    world = obj.matrix_world
    rotation = world.to_3x3()
    for polygon in mesh.polygons:
        normal = rotation @ polygon.normal
        axis = max(range(AXES), key=lambda a: abs(normal[a]))
        for loop_index in polygon.loop_indices:
            p = world @ mesh.vertices[mesh.loops[loop_index].vertex_index].co
            m = Vector((p.x * scale.x + location.x, p.y * scale.y + location.y, p.z * scale.z + location.z))
            if not box or axis == 2:
                layer.data[loop_index].uv = (m.x, m.y)
            elif axis == 0:
                layer.data[loop_index].uv = (m.y, m.z)
            else:
                layer.data[loop_index].uv = (m.x, m.z)


def simplify_material(material):
    """Image textures read the UV map directly (glTF cannot express world mapping or puddles)."""
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    for node in nodes:
        if node.type != "TEX_IMAGE":
            continue
        node.projection = "FLAT"
        for link in list(node.inputs["Vector"].links):
            links.remove(link)
        name = node.image.name.lower() if node.image else ""
        if bsdf is not None and "color" in name and "emission" not in name:
            links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
        if bsdf is not None and "roughness" in name:
            links.new(node.outputs["Color"], bsdf.inputs["Roughness"])


def decimate_scans():
    """Replaces the shared meshes of the heavy scanned models by lighter copies, still shared by
    every copy of the model."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for asset, ratio in ENGINE_DECIMATE.items():
        meshes = {child.data for root in _instances.get(asset, []) for child in root.children_recursive
                  if child.type == "MESH"}
        for mesh in meshes:
            probe = bpy.data.objects.new("DecimateProbe", mesh)
            bpy.context.collection.objects.link(probe)
            modifier = probe.modifiers.new("Decimate", "DECIMATE")
            modifier.ratio = ratio
            depsgraph.update()
            light = bpy.data.meshes.new_from_object(probe.evaluated_get(depsgraph))
            for obj in bpy.data.objects:
                if obj.data == mesh and obj is not probe:
                    obj.data = light
            bpy.data.objects.remove(probe)


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


def fit_triangle_budget(root, budget):
    """Decimates the meshes under `root` so that together they stay within `budget` triangles."""
    meshes = [obj for obj in root.children_recursive if obj.type == "MESH" and not obj.hide_render]
    for obj in meshes:
        for modifier in obj.modifiers:
            if modifier.type == "SUBSURF":
                modifier.levels = modifier.render_levels = ENGINE_SUBSURF_LEVELS
    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()
    counts = {obj: triangle_count(obj, depsgraph) for obj in meshes}
    total = sum(counts.values())
    if total <= budget:
        return
    ratio = budget / total
    for obj, count in counts.items():
        if count >= ENGINE_DECIMATE_MIN_TRIANGLES:
            obj.modifiers.new("EngineDecimate", "DECIMATE").ratio = ratio
    print(f"[showcase] {root.name}: {total} -> ~{int(total * ratio)} triangles for the engine")


def flatten_car_materials():
    """Replaces the car's procedural materials by a plain Principled BSDF the engine can show."""
    for material in bpy.data.materials:
        values = next((v for prefix, v in ENGINE_CAR_MATERIALS.items() if material.name.startswith(prefix)), None)
        if values is None:
            continue
        color, metallic, roughness = values
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        nodes.clear()
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        links.new(bsdf.outputs["BSDF"], nodes.new("ShaderNodeOutputMaterial").inputs["Surface"])


def light_up_car_lamps():
    for material in bpy.data.materials:
        glow = next((v for prefix, v in ENGINE_CAR_GLOW.items() if material.name.startswith(prefix)), None)
        if glow is None:
            continue
        color, strength = glow
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        nodes.clear()
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = strength
        links.new(bsdf.outputs["BSDF"], nodes.new("ShaderNodeOutputMaterial").inputs["Surface"])


def add_car_lights(car):
    for name, location, color, watts in ENGINE_CAR_LIGHTS:
        light = kit.point_light(name, (0.0, 0.0, 0.0), color, watts)
        light.parent = car
        light.location = location


def use_albedo_textures(obj):
    """The scans mix several maps into their skin; glTF wants the albedo straight into Base Color
    (otherwise the exporter picks the subsurface map and people come out almost black)."""
    for slot in obj.material_slots:
        material = slot.material
        if material is None or not material.use_nodes:
            continue
        nodes, links = material.node_tree.nodes, material.node_tree.links
        # Scans may carry painted vertex colors, which engines multiply into the base color.
        while obj.type == "MESH" and obj.data.color_attributes:
            obj.data.color_attributes.remove(obj.data.color_attributes[0])
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        albedo = next((n for n in nodes if n.type == "TEX_IMAGE" and n.image and "albedo" in n.image.name.lower()), None)
        if bsdf is None or albedo is None:
            continue
        links.new(albedo.outputs["Color"], bsdf.inputs["Base Color"])
        # The scans derive a bump from the photo; as a glTF normal map that makes them look chromed.
        for link in list(bsdf.inputs["Normal"].links):
            links.remove(link)
        bsdf.inputs["Roughness"].default_value = ENGINE_PERSON_ROUGHNESS


def meshes_under(root):
    return [obj for obj in root.children_recursive if obj.type == "MESH"]


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


def split_by_material(car):
    """One object per material, so painting a car (the `tint` property colors a whole object)
    does not also color the glass or trim that shared a mesh with a painted panel."""
    mixed = [obj for obj in meshes_under(car) if len({slot.material for slot in obj.material_slots if slot.material}) > 1]
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


def group_wheels(car):
    """Parents the parts of each wheel (tire, rim, brake disc) to a pivot at its axle, named
    "... Wheel N", so scripts can spin them."""
    bpy.context.view_layer.update()
    tires = [obj for obj in meshes_under(car) if obj.name.startswith(CAR_TIRE_PREFIX)]
    parts = [obj for obj in meshes_under(car) if CAR_FIXED_PART_WORD not in obj.name.lower()]
    for index, tire in enumerate(tires):
        center = world_center(tire)
        radius = max(tire.dimensions) / 2
        pivot = bpy.data.objects.new(f"{car.name} Wheel {index}", None)
        bpy.context.collection.objects.link(pivot)
        pivot.parent = car
        pivot.location = car.matrix_world.inverted() @ center
        bpy.context.view_layer.update()
        to_car = car.matrix_world.inverted().to_3x3()
        for obj in parts:
            offset = to_car @ (world_center(obj) - center)  # car space: x sideways, y along, z up
            if abs(offset.x) > ENGINE_WHEEL_HALF_WIDTH or Vector((offset.y, offset.z)).length > radius * ENGINE_WHEEL_REACH:
                continue
            if max(obj.dimensions) > radius * 2 * ENGINE_WHEEL_REACH:
                continue  # a body panel that happens to be centered near the wheel
            world = obj.matrix_world.copy()
            obj.parent = pivot
            obj.matrix_parent_inverse = pivot.matrix_world.inverted() @ world @ obj.matrix_basis.inverted()


def paint_car(car, color):
    for obj in meshes_under(car):
        if any(slot.material and slot.material.name.startswith(ENGINE_PAINT_PREFIXES) for slot in obj.material_slots):
            obj["tint"] = color


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
        for modifier in obj.modifiers:
            if modifier.type == "SUBSURF":
                modifier.levels = modifier.render_levels = 0
        if keep < 1.0 and len(obj.data.polygons) >= ENGINE_DECIMATE_MIN_TRIANGLES:
            obj.modifiers.new("FarDecimate", "DECIMATE").ratio = keep
    return light


def merge_level(car, level, visible_from, visible_to):
    """Moves the parts of `level` (another version of the same car) under `car`, drawn only between
    those distances from the camera."""
    for obj in meshes_under(level):
        obj["visible_from"] = visible_from
        if visible_to is not None:
            obj["visible_to"] = visible_to
    bpy.context.view_layer.update()
    for child in list(level.children):
        world = child.matrix_world.copy()
        child.parent = car
        child.matrix_parent_inverse = car.matrix_world.inverted() @ world @ child.matrix_basis.inverted()
    bpy.data.objects.remove(level)


def join_by_material(objects):
    """Joins the mesh objects that share a parent and a material: what the engine pays for is the
    number of things it draws, far more than their triangles."""
    groups = {}
    for obj in objects:
        material = obj.material_slots[0].material if obj.material_slots else None
        groups.setdefault((obj.parent, material), []).append(obj)
    for parts in groups.values():
        if len(parts) < 2:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for obj in parts:
            if obj.data.users > 1:
                obj.data = obj.data.copy()  # joining writes into the mesh: not into one others share
            obj.select_set(True)
        bpy.context.view_layer.objects.active = parts[0]
        bpy.ops.object.join()
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


def lighten_cast():
    cap_textures()
    flatten_car_materials()
    light_up_car_lamps()
    car = bpy.data.objects.get(root_name(HERO_CAR))
    if car is not None:
        fit_triangle_budget(car, ENGINE_CAR_TRIANGLES)
        # Three versions of the car, each drawn at its own range of distances. Every car in the
        # street is a copy of this one, so they all share the three sets of meshes.
        near_range, mid_range = ENGINE_CAR_DETAIL_RANGES
        middle = lighter_car(car, "CarMiddle", 1.0)
        far = lighter_car(car, "CarFar", ENGINE_CAR_FAR_KEEP)
        for version in (car, middle, far):
            prepare_car(version)
        smooth_shading(middle)
        smooth_shading(far)
        for obj in meshes_under(car):
            obj["visible_to"] = near_range
        merge_level(car, middle, near_range, mid_range)
        merge_level(car, far, mid_range, None)
        add_traffic(car)
        paint_car(car, ENGINE_HERO_PAINT)
        add_car_lights(car)
    for person in PEOPLE:
        root = bpy.data.objects.get(root_name(person[1]))
        if root is None:
            continue
        fit_triangle_budget(root, ENGINE_PERSON_TRIANGLES)
        bake_modifiers(root)
        for obj in root.children_recursive:
            use_albedo_textures(obj)
        if person[1] in ENGINE_WALKERS:  # along the sidewalk, on legs that really walk
            heading, x = ENGINE_WALKERS[person[1]]
            root.rotation_euler = (0.0, 0.0, math.radians(heading))
            root.location.x = x
            bodies = meshes_under(root)
            if len(bodies) != 1:
                raise SystemExit(f"{person[1]}: expected one mesh to rig, found {len(bodies)}")
            _, speed = walk_rig.rig_walker(bodies[0], person[1])
            root["speed"] = speed  # a fact of the rig: the pace at which its feet do not slide


def deform_road():
    """Replaces the flat road by a worn one: a grid with a crown, broad waves and a few dips."""
    old = bpy.data.objects.get("Road")
    if old is None:
        return
    material = old.active_material
    bpy.data.objects.remove(old)
    half = ROAD_WIDTH / 2
    columns = round(ROAD_WIDTH / ENGINE_ROAD_CELL)
    rows = round(STREET_LENGTH / ENGINE_ROAD_CELL)
    vertices = []
    for row in range(rows + 1):
        y = STREET_START + STREET_LENGTH * row / rows
        for column in range(columns + 1):
            x = -half + ROAD_WIDTH * column / columns
            wave = noise.noise(Vector((x / ENGINE_ROAD_WAVE_SIZE, y / ENGINE_ROAD_WAVE_SIZE, 0.0)))  # -1..1
            dip = noise.noise(Vector((x / ENGINE_ROAD_DIP_SIZE, y / ENGINE_ROAD_DIP_SIZE, NOISE_LAYER_DIPS))) * 0.5 + 0.5
            dip = max(dip - ENGINE_ROAD_DIP_THRESHOLD, 0.0) / (1.0 - ENGINE_ROAD_DIP_THRESHOLD)
            crown = ENGINE_ROAD_CROWN * (abs(x) / half) ** 2
            z = -crown - ENGINE_ROAD_WAVE_DEPTH * (wave * 0.5 + 0.5) - ENGINE_ROAD_DIP_DEPTH * dip
            for hole_x, hole_y, radius, depth in ENGINE_POTHOLES:
                ragged = radius * (1.0 + ENGINE_POTHOLE_EDGE_NOISE * noise.noise(
                    Vector((x / ENGINE_POTHOLE_EDGE_SCALE, y / ENGINE_POTHOLE_EDGE_SCALE, NOISE_LAYER_POTHOLES))))
                inside = 1.0 - math.hypot(x - hole_x, y - hole_y) / ragged
                if inside > 0.0:
                    z -= depth * min(inside * ENGINE_POTHOLE_WALL_STEEPNESS, 1.0)  # steep walls, flat bottom
            vertices.append((x, y, z))
    faces = []
    for row in range(rows):
        for column in range(columns):
            a = row * (columns + 1) + column
            faces.append((a, a + 1, a + columns + 2, a + columns + 1))
    mesh = bpy.data.meshes.new("Road")
    mesh.from_pydata(vertices, [], faces)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    road = bpy.data.objects.new("Road", mesh)
    bpy.context.collection.objects.link(road)
    road.data.materials.append(material)
    for index, (x, y, radius, depth) in enumerate(ENGINE_POTHOLES):
        kit.empty(f"Pothole{index + 1}", (x, y, 0.0), props={"tag": POTHOLE_TAG, "radius": radius, "depth": depth})


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
        leaf["tint"] = color


def join_building_parts():
    """Each building's boxes (plinth, cornice, shop band, windows, frames, awnings) become one
    object per material."""
    buildings = [obj for obj in bpy.data.objects if obj.name.startswith("Building") and obj.name.count("_") == 1]
    for building in buildings:
        prefix = building.name + "_"
        join_by_material([obj for obj in bpy.data.objects
                          if obj.type == "MESH" and obj.name.startswith(prefix) and obj.parent is None])


def dress_for_engine():
    """Sunset mood with what the engine has: an environment object, a warm sun, brighter street
    lamps."""
    environment = bpy.data.objects.new("Environment", None)
    bpy.context.collection.objects.link(environment)
    for key, value in ENGINE_ENVIRONMENT.items():
        environment[key] = value
    for obj in bpy.data.objects:
        if obj.type == "LIGHT" and obj.name.startswith("LampLight"):
            obj.data.energy = ENGINE_LAMP_WATTS


def export_for_engine():
    if bpy.data.objects.get(root_name(HERO_CAR)) is None:
        # The game (and its test) is about driving the car.
        raise SystemExit("the car is missing: run python3 -I tools/assets/fetch_ad_assets.py before exporting")
    haze = bpy.data.objects.get("Haze")
    if haze is not None:
        bpy.data.objects.remove(haze)
    deform_road()  # before the UV bake below, which reads the road's vertices
    for obj in bpy.context.scene.objects:
        bake_world_uvs(obj)
    for material in bpy.data.materials:
        if world_mapping_node(material) is not None:
            simplify_material(material)
    decimate_scans()
    lighten_cast()
    join_building_parts()
    # The sky's sun becomes a real light (the engine does not light with HDRIs).
    sun_location = (math.cos(SUN_TARGET_AZIMUTH) * ENGINE_SUN_DISTANCE,
                    math.sin(SUN_TARGET_AZIMUTH) * ENGINE_SUN_DISTANCE, ENGINE_SUN_HEIGHT)
    kit.sun("Sun", sun_location, (0.0, 0.0, 0.0), ENGINE_SUN_STRENGTH).data.color = ENGINE_SUN_COLOR
    dress_for_engine()
    add_leaves()
    car = bpy.data.objects[root_name(HERO_CAR)]
    car.rotation_euler = (0.0, 0.0, math.radians(ENGINE_CAR_HEADING))
    car.location.x = ENGINE_LANE
    os.makedirs(GAME_DIR, exist_ok=True)
    kit.export_glb(os.path.join(GAME_DIR, "scene.glb"))


def main():
    out_dir = kit.output_path_from_argv("showcase")
    for needed in (MODELS, TEXTURES, HDRI):
        if not os.path.exists(needed):
            raise SystemExit(f"{needed} is missing: run python3 -I tools/assets/fetch_showcase.py first")
    kit.reset_scene()
    rng = random.Random(SEED)
    build_street()
    shop_fronts = build_buildings(rng)
    build_street_furniture(rng)
    build_shop_fronts(shop_fronts, random.Random(SEED + SHOP_SEED_OFFSET))
    build_world()
    build_haze()
    build_cast()
    build_camera()
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
