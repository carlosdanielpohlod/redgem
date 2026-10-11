"""Gives a scanned standing person a leg skeleton and a looping walk, so game engines can animate
them. The scans have no rig; their legs are roughly vertical, which makes simple height-based
skinning work: hips and everything above stay rigid, each leg bends at hip, knee and ankle.

Used by build_showcase.py; everything is in the mesh's own space (z up, the person faces -Y).
"""

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

# Body proportions as fractions of the person's height.
CROTCH = 0.47
HIP_TOP = 0.56          # above this the body is rigid
KNEE = 0.27
ANKLE = 0.055
JOINT_BLEND = 0.035     # half-height of the zone where two bones share a joint's vertices
TOE_LENGTH = 0.09
TOE_HEIGHT = 0.017
HEAD_TOP = 1.0
# Arms hanging by the body are not legs: only this close to a leg's axis counts (fraction of height).
LEG_REACH = 0.085
# Materials of things carried or attached to the upper body, even when they hang low.
RIGID_MATERIAL_WORDS = ("hand", "phone", "b_low", "bag")

# The walk (one cycle = two steps).
WALK_FRAMES = 30
FRAME_RATE = 24
THIGH_SWING = math.radians(27.0)
KNEE_LIFT = math.radians(58.0)       # extra knee bend while the leg swings forward
KNEE_STANCE = math.radians(6.0)      # legs are never perfectly straight
KNEE_LIFT_SHARPNESS = 1.5            # how late in the swing the knee bends (1 = evenly)
FOOT_LEVELING = 0.75                 # how much the foot cancels the leg's tilt
HIP_BOB = 0.012                      # fraction of height the hips drop twice per cycle
HIP_SWAY = math.radians(2.5)
SPINE_COUNTER = math.radians(3.0)    # shoulders turn against the hips
KEY_EVERY = 2
STEPS_PER_CYCLE = 2
SIDES = (("L", 1.0), ("R", -1.0))
FULL_TURN = math.tau                 # radians in one walk cycle
HALF_TURN = math.pi                  # the two legs are half a cycle apart
SWINGS_PER_STEP = 2.0                # a step spans the swing forward plus the swing back
JOINT_ZONE = 2.0 * JOINT_BLEND       # full height of the zone two bones share
# Bone names. Each leg bone exists once per side: "Thigh.L", "Thigh.R", ...
HIPS = "Hips"
SPINE = "Spine"
THIGH = "Thigh"
SHIN = "Shin"
FOOT = "Foot"
# Going down a leg: (bone above the joint, bone below it, height of the joint).
LEG_JOINTS = ((THIGH, SHIN, KNEE), (SHIN, FOOT, ANKLE))
# The curve 3t^2 - 2t^3: eases in and out between 0 and 1.
EASE_SQUARE = 3.0
EASE_CUBE = 2.0
QUATERNION = "QUATERNION"
TRANSFORM_SIZE = 4                   # rows and columns of a placement matrix
ROTATION_PATH = "rotation_quaternion"   # the pose property that gets keyframes
LOCATION_PATH = "location"
LATERAL = Vector((1.0, 0.0, 0.0))    # legs swing around this axis
UPRIGHT = Vector((0.0, 0.0, 1.0))


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (EASE_SQUARE - EASE_CUBE * t)


def side_bone(part, side):
    return f"{part}.{side}"


def measure(mesh_object):
    """Height, center line and leg axes of the person."""
    zs = [v.co.z for v in mesh_object.data.vertices]
    height = max(zs) - min(zs)
    floor = min(zs)
    knee_band = [v.co for v in mesh_object.data.vertices if abs((v.co.z - floor) / height - KNEE) < JOINT_BLEND]
    center_x = sum(co.x for co in knee_band) / len(knee_band)
    center_y = sum(co.y for co in knee_band) / len(knee_band)
    spread = sum(abs(co.x - center_x) for co in knee_band) / len(knee_band)
    return {"height": height, "floor": floor, "x": center_x, "y": center_y, "leg": spread}


def body_point(body, x, fraction, y=None):
    """The point at `fraction` of the person's height, on the center line unless `y` is given."""
    return Vector((x, body["y"] if y is None else y, body["floor"] + body["height"] * fraction))


def add_bone(data, name, head, tail, parent=None):
    bone = data.edit_bones.new(name)
    bone.head, bone.tail = head, tail
    if parent is not None:
        bone.parent = parent
    return bone


def add_leg_bones(data, body, hips, side, sign):
    x = body["x"] + sign * body["leg"]
    toe_y = body["y"] - body["height"] * TOE_LENGTH
    thigh = add_bone(data, side_bone(THIGH, side), body_point(body, x, CROTCH + JOINT_BLEND),
                     body_point(body, x, KNEE), hips)
    shin = add_bone(data, side_bone(SHIN, side), body_point(body, x, KNEE), body_point(body, x, ANKLE), thigh)
    add_bone(data, side_bone(FOOT, side), body_point(body, x, ANKLE), body_point(body, x, TOE_HEIGHT, toe_y), shin)


def build_armature(name, body):
    center = body["x"]
    data = bpy.data.armatures.new(name)
    armature = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    hips = add_bone(data, HIPS, body_point(body, center, CROTCH), body_point(body, center, HIP_TOP))
    add_bone(data, SPINE, body_point(body, center, HIP_TOP), body_point(body, center, HEAD_TOP), hips)
    for side, sign in SIDES:
        add_leg_bones(data, body, hips, side, sign)
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def rigid_vertices(mesh_object):
    """Vertices of carried things (phone, bag, hands), which follow the upper body."""
    rigid_slots = {index for index, slot in enumerate(mesh_object.material_slots)
                   if slot.material and any(word in slot.material.name.lower() for word in RIGID_MATERIAL_WORDS)}
    rigid = set()
    for polygon in mesh_object.data.polygons:
        if polygon.material_index in rigid_slots:
            rigid.update(polygon.vertices)
    return rigid


def leg_weights(fraction, side):
    """Bone name -> weight for a point of a leg, below the crotch."""
    for upper, lower, joint in LEG_JOINTS:
        if fraction >= joint + JOINT_BLEND:
            return {side_bone(upper, side): 1.0}
        if fraction > joint - JOINT_BLEND:
            down = smooth((joint + JOINT_BLEND - fraction) / JOINT_ZONE)
            return {side_bone(upper, side): 1.0 - down, side_bone(lower, side): down}
    return {side_bone(FOOT, side): 1.0}


def vertex_weights(co, body):
    """Bone name -> weight for a point of the body."""
    fraction = (co.z - body["floor"]) / body["height"]
    side, sign = SIDES[0] if co.x >= body["x"] else SIDES[1]
    off_axis = abs(co.x - (body["x"] + sign * body["leg"])) / body["height"]
    if fraction >= HIP_TOP or (fraction > KNEE and off_axis > LEG_REACH):
        return {SPINE: 1.0}
    if fraction >= CROTCH:
        up = smooth((fraction - CROTCH) / (HIP_TOP - CROTCH))
        return {SPINE: up, side_bone(THIGH, side): 1.0 - up}
    return leg_weights(fraction, side)


def skin(mesh_object, armature, body):
    groups = {bone.name: mesh_object.vertex_groups.new(name=bone.name) for bone in armature.data.bones}
    rigid = rigid_vertices(mesh_object)
    for vertex in mesh_object.data.vertices:
        weights = {SPINE: 1.0} if vertex.index in rigid else vertex_weights(vertex.co, body)
        for bone, weight in weights.items():
            if weight > 0.0:
                groups[bone].add([vertex.index], weight, "REPLACE")
    mesh_object.modifiers.new("Armature", "ARMATURE").object = armature


def lateral_turn(bone, angle):
    """Pose rotation that turns `bone` by `angle` around the sideways axis (forward swing positive)."""
    rest = bone.bone.matrix_local.to_quaternion()
    return rest.inverted() @ Quaternion(LATERAL, -angle) @ rest


def upright_turn(bone, angle):
    rest = bone.bone.matrix_local.to_quaternion()
    return rest.inverted() @ Quaternion(UPRIGHT, angle) @ rest


def pose_leg(bones, side, leg_phase):
    thigh, shin, foot = (bones[side_bone(part, side)] for part in (THIGH, SHIN, FOOT))
    thigh_angle = THIGH_SWING * math.sin(leg_phase)
    knee_angle = KNEE_STANCE + KNEE_LIFT * max(math.cos(leg_phase), 0.0) ** KNEE_LIFT_SHARPNESS
    thigh.rotation_quaternion = lateral_turn(thigh, thigh_angle)
    shin.rotation_quaternion = lateral_turn(shin, -knee_angle)
    foot.rotation_quaternion = lateral_turn(foot, -(thigh_angle - knee_angle) * FOOT_LEVELING)


def pose_torso(bones, body, phase):
    hips, spine = bones[HIPS], bones[SPINE]
    sway = math.sin(phase)
    hips.rotation_quaternion = upright_turn(hips, HIP_SWAY * sway)
    hips.location = hips.bone.matrix_local.to_3x3().inverted() @ Vector(
        (0.0, 0.0, -HIP_BOB * body["height"] * abs(math.cos(phase))))
    spine.rotation_quaternion = upright_turn(spine, -(HIP_SWAY + SPINE_COUNTER) * sway)


def key_pose(bones, frame):
    for bone in bones:
        bone.keyframe_insert(ROTATION_PATH, frame=frame)
    bones[HIPS].keyframe_insert(LOCATION_PATH, frame=frame)


def animate_walk(armature, body, action_name):
    armature.animation_data_create()
    action = bpy.data.actions.new(action_name)
    armature.animation_data.action = action
    bones = armature.pose.bones
    for bone in bones:
        bone.rotation_mode = QUATERNION
    for frame in range(0, WALK_FRAMES + 1, KEY_EVERY):
        phase = FULL_TURN * frame / WALK_FRAMES
        for index, (side, _) in enumerate(SIDES):
            pose_leg(bones, side, phase + HALF_TURN * index)
        pose_torso(bones, body, phase)
        key_pose(bones, frame)
    return action


def cycle_seconds():
    return WALK_FRAMES / FRAME_RATE


def stride_speed(body, scale=1.0):
    """Ground speed (m/s) at which the walking feet slide least: two steps per cycle, each as
    long as the swing of a straight leg hinged at the hip. `scale` is the model's size in the
    scene (its vertical scale), since `body` is measured in the mesh's own units."""
    leg = body["height"] * (CROTCH + JOINT_BLEND - ANKLE)  # hip joint to ankle
    step = SWINGS_PER_STEP * leg * math.sin(THIGH_SWING)
    return STEPS_PER_CYCLE * step / cycle_seconds() * scale


def take_place_of(armature, mesh_object):
    """The armature takes the mesh's place in the hierarchy; the mesh follows it.
    (Copying the local placement, not matrix_world, which Blender only refreshes on demand.)"""
    armature.parent = mesh_object.parent
    armature.matrix_parent_inverse = mesh_object.matrix_parent_inverse.copy()
    armature.matrix_basis = mesh_object.matrix_basis.copy()
    mesh_object.parent = armature
    mesh_object.matrix_parent_inverse = Matrix.Identity(TRANSFORM_SIZE)
    mesh_object.location = (0.0, 0.0, 0.0)
    mesh_object.rotation_euler = (0.0, 0.0, 0.0)
    mesh_object.scale = (1.0, 1.0, 1.0)


def rig_walker(mesh_object, name):
    """Adds armature, skinning and a walk action to `mesh_object`. Returns (armature, speed m/s).
    Sets the scene's frame rate to the walk's (glTF exports keyframe times from it)."""
    body = measure(mesh_object)
    armature = build_armature(f"{name} Rig", body)
    skin(mesh_object, armature, body)
    take_place_of(armature, mesh_object)
    animate_walk(armature, body, f"{name} Walk")
    bpy.context.scene.render.fps = FRAME_RATE
    return armature, stride_speed(body, mesh_object.matrix_world.to_scale().z)
