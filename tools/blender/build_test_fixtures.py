"""Builds the .glb fixtures used by the C++ test suites.

    tools/blender/run.sh build_test_fixtures.py tests/fixtures
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

GRAY = (0.5, 0.5, 0.5)
RED = (0.8, 0.1, 0.1)
GOLD = (1.0, 0.75, 0.1)
BLUE = (0.1, 0.3, 0.9)

GROUND_SIZE = (20.0, 20.0, 0.5)
GROUND_TOP = 0.0
QUARTER_TURN = math.radians(90.0)
BEEP_RATE = 8000
BEEP_FRAMES = 400
BEEP_SAMPLE_BYTES = 2
ARM_LENGTH = 4.0
ARM_WIDTH = 1.0
WAVE_FPS = 24


def build_hierarchy(path):
    """Parent/child transforms and every kind of custom property value."""
    kit.reset_scene()
    root = kit.empty("Root", (1.0, 2.0, 3.0), props={
        "script": "spinner.rb",
        "speed": 2.5,
        "enabled": True,
        "label": "hello",
        "tag": "thing",
        "count": 3,
    })
    kit.box("Child", (0.0, 0.0, 1.0), (2.0, 2.0, 2.0), mat=kit.material("Red", RED), parent=root)
    kit.camera("Camera", (0.0, -10.0, 5.0), (0.0, 0.0, 0.0))
    kit.sun("Sun", (0.0, 0.0, 10.0), (1.0, 1.0, 0.0))
    kit.export_glb(path)


def build_sandbox(path):
    """A small physics playground driven by probe scripts in the integration tests."""
    kit.reset_scene()
    ground_center_z = GROUND_TOP - GROUND_SIZE[2] / 2
    kit.box("Ground", (0.0, 0.0, ground_center_z), GROUND_SIZE, mat=kit.material("Gray", GRAY),
            props={"collider": "box"})
    kit.box("Crate", (5.0, 0.0, 3.0), (1.0, 1.0, 1.0), mat=kit.material("Red", RED),
            props={"body": "dynamic", "mass": 2.0, "tag": "crate"})
    kit.box("Hero", (0.0, 0.0, 1.2), (0.8, 0.8, 2.0), mat=kit.material("Blue", BLUE),
            props={"body": "character", "script": "probe.rb", "speed": 4.0})
    kit.cylinder("Coin", (3.0, 0.0, 1.0), 0.4, 0.1, mat=kit.material("Gold", GOLD),
                 rotation=(QUARTER_TURN, 0.0, 0.0),
                 props={"trigger": True, "script": "coin_probe.rb", "tag": "coin"})
    kit.box("Lift", (-5.0, 0.0, 0.5), (2.0, 2.0, 0.2), mat=kit.material("Gray", GRAY),
            props={"body": "kinematic", "script": "lift.rb"})
    family = kit.empty("Family", (8.0, 8.0, 1.0), props={"script": "family.rb"})
    kit.box("Kid", (0.0, 0.0, 0.0), (0.5, 0.5, 0.5), mat=kit.material("Blue", BLUE), parent=family,
            props={"script": "kid.rb"})
    kit.empty("GcStress", (0.0, -5.0, 0.0), props={"script": "gc_stress.rb"})
    # A trigger resting on the ground: static geometry must not fire it.
    kit.box("FloorSensor", (-8.0, -8.0, 0.25), (1.0, 1.0, 1.0), props={"trigger": True, "script": "sensor.rb"})
    kit.sphere("Bullet", (0.0, -8.0, 3.0), 0.25, mat=kit.material("Red", RED),
               props={"prefab": True, "body": "dynamic", "collider": "sphere", "script": "bullet.rb", "tag": "bullet"})
    kit.empty("Spawner", (0.0, -8.0, 0.0), props={"script": "spawner.rb"})
    kit.empty("Broken", (0.0, 5.0, 0.0), props={"script": "broken.rb"})
    kit.empty("Missing", (0.0, 6.0, 0.0), props={"script": "does_not_exist.rb"})
    kit.camera("Camera", (0.0, -12.0, 6.0), (0.0, 0.0, 0.0))
    kit.sun("Sun", (0.0, 0.0, 10.0), (1.0, 1.0, 0.0))
    kit.export_glb(path)


def build_skinned(path):
    """A two-bone arm: a skinned box whose upper half bends, with a one-second "Wave" animation."""
    import bpy
    kit.reset_scene()
    arm = kit.box("Arm", (0.0, 0.0, ARM_LENGTH / 2), (ARM_WIDTH, ARM_WIDTH, ARM_LENGTH), mat=kit.material("Red", RED),
                  bake_scale=True)
    data = bpy.data.armatures.new("Rig")
    rig = bpy.data.objects.new("Rig", data)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    lower = data.edit_bones.new("Lower")
    lower.head, lower.tail = (0.0, 0.0, 0.0), (0.0, 0.0, ARM_LENGTH / 2)
    upper = data.edit_bones.new("Upper")
    upper.head, upper.tail = (0.0, 0.0, ARM_LENGTH / 2), (0.0, 0.0, ARM_LENGTH)
    upper.parent = lower
    bpy.ops.object.mode_set(mode="OBJECT")
    groups = {name: arm.vertex_groups.new(name=name) for name in ("Lower", "Upper")}
    for vertex in arm.data.vertices:  # the box's top corners follow the upper bone
        groups["Upper" if (arm.matrix_world @ vertex.co).z > ARM_LENGTH / 2 else "Lower"].add([vertex.index], 1.0, "REPLACE")
    arm.parent = rig
    arm.modifiers.new("Armature", "ARMATURE").object = rig
    rig.animation_data_create()
    rig.animation_data.action = bpy.data.actions.new("Wave")
    bone = rig.pose.bones["Upper"]
    bone.rotation_mode = "XYZ"
    bpy.context.scene.render.fps = WAVE_FPS
    for frame, angle in ((0, 0.0), (WAVE_FPS, QUARTER_TURN)):
        bone.rotation_euler = (angle, 0.0, 0.0)
        bone.keyframe_insert("rotation_euler", frame=frame)
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 0, WAVE_FPS
    kit.camera("Camera", (0.0, -12.0, 2.0), (0.0, 0.0, 2.0))
    kit.export_glb(path)


def build_sandbox_level2(path):
    """Second scene of the sandbox, loaded with Game.load_scene("level2")."""
    kit.reset_scene()
    kit.box("Level2Ground", (0.0, 0.0, -0.25), GROUND_SIZE, mat=kit.material("Gray", GRAY), props={"collider": "box"})
    kit.empty("Level2Marker", (0.0, 0.0, 1.0), props={"script": "level2_marker.rb"})
    kit.camera("Camera", (0.0, -12.0, 6.0), (0.0, 0.0, 0.0))
    kit.export_glb(path)


def write_beep(path):
    """A short silent WAV so audio lookups have a real file to find."""
    import wave
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as beep:
        beep.setnchannels(1)
        beep.setsampwidth(BEEP_SAMPLE_BYTES)
        beep.setframerate(BEEP_RATE)
        beep.writeframes(b"\x00" * BEEP_FRAMES * BEEP_SAMPLE_BYTES)


def main():
    out_dir = kit.output_path_from_argv("tests/fixtures")
    build_hierarchy(os.path.join(out_dir, "hierarchy.glb"))
    build_skinned(os.path.join(out_dir, "skinned.glb"))
    build_sandbox(os.path.join(out_dir, "games", "sandbox", "scene.glb"))
    build_sandbox_level2(os.path.join(out_dir, "games", "sandbox", "scenes", "level2.glb"))
    write_beep(os.path.join(out_dir, "games", "sandbox", "sounds", "beep.wav"))


main()
