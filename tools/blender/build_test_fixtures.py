"""Builds the .glb fixtures used by the C++ test suites.

    tools/blender/run.sh build_test_fixtures.py tests/fixtures
"""

import math
import os
import sys
import wave

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

# Material name -> color (linear RGB).
COLORS = {
    "Gray": (0.5, 0.5, 0.5),
    "Red": (0.8, 0.1, 0.1),
    "Gold": (1.0, 0.75, 0.1),
    "Blue": (0.1, 0.3, 0.9),
}

ORIGIN = (0.0, 0.0, 0.0)
GROUND_SIZE = (20.0, 20.0, 0.5)   # meters
GROUND_TOP = 0.0
GROUND_CENTER = (0.0, 0.0, GROUND_TOP - GROUND_SIZE[2] / 2)
QUARTER_TURN = math.radians(90.0)
SUN_LOCATION = (0.0, 0.0, 10.0)
SUN_TARGET = (1.0, 1.0, 0.0)

# Scenes are lists of objects, built in this order. Each has a "shape" (box, cylinder, sphere or
# empty), a name, a location in meters and what that shape needs; "material" is a key of COLORS,
# "parent" the name of an earlier object, "props" the custom properties the engine reads.
HIERARCHY_OBJECTS = (
    {"shape": "empty", "name": "Root", "location": (1.0, 2.0, 3.0),
     "props": {"script": "spinner.rb", "speed": 2.5, "enabled": True, "label": "hello", "tag": "thing", "count": 3}},
    {"shape": "box", "name": "Child", "location": (0.0, 0.0, 1.0), "size": (2.0, 2.0, 2.0), "material": "Red",
     "parent": "Root"},
)
HIERARCHY_CAMERA = (0.0, -10.0, 5.0)

SANDBOX_OBJECTS = (
    {"shape": "box", "name": "Ground", "location": GROUND_CENTER, "size": GROUND_SIZE, "material": "Gray",
     "props": {"collider": "box"}},
    {"shape": "box", "name": "Crate", "location": (5.0, 0.0, 3.0), "size": (1.0, 1.0, 1.0), "material": "Red",
     "props": {"body": "dynamic", "mass": 2.0, "tag": "crate"}},
    {"shape": "box", "name": "Hero", "location": (0.0, 0.0, 1.2), "size": (0.8, 0.8, 2.0), "material": "Blue",
     "props": {"body": "character", "script": "probe.rb", "speed": 4.0}},
    {"shape": "cylinder", "name": "Coin", "location": (3.0, 0.0, 1.0), "radius": 0.4, "depth": 0.1,
     "material": "Gold", "rotation": (QUARTER_TURN, 0.0, 0.0),
     "props": {"trigger": True, "script": "coin_probe.rb", "tag": "coin"}},
    {"shape": "box", "name": "Lift", "location": (-5.0, 0.0, 0.5), "size": (2.0, 2.0, 0.2), "material": "Gray",
     "props": {"body": "kinematic", "script": "lift.rb"}},
    {"shape": "empty", "name": "Family", "location": (8.0, 8.0, 1.0), "props": {"script": "family.rb"}},
    {"shape": "box", "name": "Kid", "location": ORIGIN, "size": (0.5, 0.5, 0.5), "material": "Blue",
     "parent": "Family", "props": {"script": "kid.rb"}},
    {"shape": "empty", "name": "GcStress", "location": (0.0, -5.0, 0.0), "props": {"script": "gc_stress.rb"}},
    # A trigger resting on the ground: static geometry must not fire it.
    {"shape": "box", "name": "FloorSensor", "location": (-8.0, -8.0, 0.25), "size": (1.0, 1.0, 1.0),
     "props": {"trigger": True, "script": "sensor.rb"}},
    {"shape": "sphere", "name": "Bullet", "location": (0.0, -8.0, 3.0), "radius": 0.25, "material": "Red",
     "props": {"prefab": True, "body": "dynamic", "collider": "sphere", "script": "bullet.rb", "tag": "bullet"}},
    {"shape": "empty", "name": "Spawner", "location": (0.0, -8.0, 0.0), "props": {"script": "spawner.rb"}},
    {"shape": "empty", "name": "Broken", "location": (0.0, 5.0, 0.0), "props": {"script": "broken.rb"}},
    {"shape": "empty", "name": "Missing", "location": (0.0, 6.0, 0.0), "props": {"script": "does_not_exist.rb"}},
)
SANDBOX_CAMERA = (0.0, -12.0, 6.0)

LEVEL2_OBJECTS = (
    {"shape": "box", "name": "Level2Ground", "location": GROUND_CENTER, "size": GROUND_SIZE, "material": "Gray",
     "props": {"collider": "box"}},
    {"shape": "empty", "name": "Level2Marker", "location": (0.0, 0.0, 1.0), "props": {"script": "level2_marker.rb"}},
)

# The skinned arm: a box standing on the origin with a bone in each half (meters).
ARM_LENGTH = 4.0
ARM_WIDTH = 1.0
ELBOW_HEIGHT = ARM_LENGTH / 2
ARM_MATERIAL = "Red"
LOWER_BONE = "Lower"
UPPER_BONE = "Upper"
WAVE_FPS = 24                       # the "Wave" animation lasts one second
ARM_CAMERA = (0.0, -12.0, 2.0)
ARM_CAMERA_TARGET = (0.0, 0.0, 2.0)

BEEP_RATE = 8000                    # samples per second
BEEP_FRAMES = 400
BEEP_SAMPLE_BYTES = 2
BEEP_SILENCE = b"\x00"


def paint(name):
    """The material called `name`, or None for an object without one."""
    return kit.material(name, COLORS[name]) if name else None


def add_box(spec, mat, parent):
    return kit.box(spec["name"], spec["location"], spec["size"], mat=mat, props=spec.get("props"), parent=parent)


def add_cylinder(spec, mat, parent):
    return kit.cylinder(spec["name"], spec["location"], spec["radius"], spec["depth"], mat=mat,
                        rotation=spec["rotation"], props=spec.get("props"), parent=parent)


def add_sphere(spec, mat, parent):
    return kit.sphere(spec["name"], spec["location"], spec["radius"], mat=mat, props=spec.get("props"), parent=parent)


def add_empty(spec, _mat, parent):
    return kit.empty(spec["name"], spec["location"], props=spec.get("props"), parent=parent)


SHAPES = {"box": add_box, "cylinder": add_cylinder, "sphere": add_sphere, "empty": add_empty}


def build_objects(specs):
    built = {}
    for spec in specs:
        mat = paint(spec.get("material"))
        built[spec["name"]] = SHAPES[spec["shape"]](spec, mat, built.get(spec.get("parent")))
    return built


def build_hierarchy(path):
    """Parent/child transforms and every kind of custom property value."""
    kit.reset_scene()
    build_objects(HIERARCHY_OBJECTS)
    kit.camera("Camera", HIERARCHY_CAMERA, ORIGIN)
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET)
    kit.export_glb(path)


def build_sandbox(path):
    """A small physics playground driven by probe scripts in the integration tests."""
    kit.reset_scene()
    build_objects(SANDBOX_OBJECTS)
    kit.camera("Camera", SANDBOX_CAMERA, ORIGIN)
    kit.sun("Sun", SUN_LOCATION, SUN_TARGET)
    kit.export_glb(path)


def build_arm_rig():
    """An armature with two bones end to end: "Lower" up to the elbow, "Upper" above it."""
    data = bpy.data.armatures.new("Rig")
    rig = bpy.data.objects.new("Rig", data)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    lower = data.edit_bones.new(LOWER_BONE)
    lower.head, lower.tail = ORIGIN, (0.0, 0.0, ELBOW_HEIGHT)
    upper = data.edit_bones.new(UPPER_BONE)
    upper.head, upper.tail = (0.0, 0.0, ELBOW_HEIGHT), (0.0, 0.0, ARM_LENGTH)
    upper.parent = lower
    bpy.ops.object.mode_set(mode="OBJECT")
    return rig


def skin_arm(arm, rig):
    groups = {name: arm.vertex_groups.new(name=name) for name in (LOWER_BONE, UPPER_BONE)}
    for vertex in arm.data.vertices:  # the box's top corners follow the upper bone
        bone = UPPER_BONE if (arm.matrix_world @ vertex.co).z > ELBOW_HEIGHT else LOWER_BONE
        groups[bone].add([vertex.index], 1.0, "REPLACE")
    arm.parent = rig
    arm.modifiers.new("Armature", "ARMATURE").object = rig


def animate_wave(rig):
    """The upper bone bends a quarter turn over one second."""
    rig.animation_data_create()
    rig.animation_data.action = bpy.data.actions.new("Wave")
    bone = rig.pose.bones[UPPER_BONE]
    bone.rotation_mode = "XYZ"
    bpy.context.scene.render.fps = WAVE_FPS
    for frame, angle in ((0, 0.0), (WAVE_FPS, QUARTER_TURN)):
        bone.rotation_euler = (angle, 0.0, 0.0)
        bone.keyframe_insert("rotation_euler", frame=frame)
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 0, WAVE_FPS


def build_skinned(path):
    """A two-bone arm: a skinned box whose upper half bends, with a one-second "Wave" animation."""
    kit.reset_scene()
    arm = kit.box("Arm", (0.0, 0.0, ELBOW_HEIGHT), (ARM_WIDTH, ARM_WIDTH, ARM_LENGTH), mat=paint(ARM_MATERIAL),
                  bake_scale=True)
    rig = build_arm_rig()
    skin_arm(arm, rig)
    animate_wave(rig)
    kit.camera("Camera", ARM_CAMERA, ARM_CAMERA_TARGET)
    kit.export_glb(path)


def build_sandbox_level2(path):
    """Second scene of the sandbox, loaded with Game.load_scene("level2")."""
    kit.reset_scene()
    build_objects(LEVEL2_OBJECTS)
    kit.camera("Camera", SANDBOX_CAMERA, ORIGIN)
    kit.export_glb(path)


def write_beep(path):
    """A short silent WAV so audio lookups have a real file to find."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as beep:
        beep.setnchannels(1)
        beep.setsampwidth(BEEP_SAMPLE_BYTES)
        beep.setframerate(BEEP_RATE)
        beep.writeframes(BEEP_SILENCE * BEEP_FRAMES * BEEP_SAMPLE_BYTES)


def main():
    out_dir = kit.output_path_from_argv("tests/fixtures")
    build_hierarchy(os.path.join(out_dir, "hierarchy.glb"))
    build_skinned(os.path.join(out_dir, "skinned.glb"))
    build_sandbox(os.path.join(out_dir, "games", "sandbox", "scene.glb"))
    build_sandbox_level2(os.path.join(out_dir, "games", "sandbox", "scenes", "level2.glb"))
    write_beep(os.path.join(out_dir, "games", "sandbox", "sounds", "beep.wav"))


main()
