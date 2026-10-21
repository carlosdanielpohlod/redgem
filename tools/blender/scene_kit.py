"""Helpers to build RedGem scenes with Blender's Python API (bpy).

Every object gets its gameplay data as Blender custom properties, which the glTF
exporter writes to the node "extras" that the engine reads.

Coordinates here are Blender's (Z up, +Y forward). The glTF exporter converts them
to the engine's Y-up space: Blender (x, y, z) -> engine (x, z, -y).
"""

import math
import os
import sys

import bpy
from mathutils import Vector

CUBE_SIZE = 1.0
DEFAULT_SEGMENTS = 32
DEFAULT_RING_COUNT = 16
SUN_STRENGTH = 3.0
POINT_LIGHT_WATTS = 600.0
CAMERA_FOV_DEGREES = 50.0
OPAQUE = 1.0
DEFAULT_ROUGHNESS = 0.6
DEFAULT_METALLIC = 0.0
WATER_IOR = 1.333
WATER_ROUGHNESS = 0.08
WATER_COLOR = (0.015, 0.08, 0.1)
WATER_PROPERTIES = {
    "water": True, "wave_height": 0.08, "wavelength": 3.0, "wave_speed": 1.0,
    "absorption_red": 0.35, "absorption_green": 0.08, "absorption_blue": 0.035,
    "foam_width": 0.12, "refraction": 0.035,
}
NO_ROTATION = (0.0, 0.0, 0.0)   # Euler angles, radians
# Cameras and lights look along their own -Z with +Y up (Blender's convention).
LOOK_AXIS = "-Z"
UP_AXIS = "Y"
PRINCIPLED_BSDF = "Principled BSDF"   # the shader node every new material starts with


def output_path_from_argv(default_path):
    """Path passed after `--` on the Blender command line, or the default."""
    if "--" in sys.argv:
        extra = sys.argv[sys.argv.index("--") + 1:]
        if extra:
            return os.path.abspath(extra[0])
    return os.path.abspath(default_path)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, rgb, roughness=DEFAULT_ROUGHNESS, metallic=DEFAULT_METALLIC):
    existing = bpy.data.materials.get(name)
    if existing:
        return existing
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes[PRINCIPLED_BSDF]
    bsdf.inputs["Base Color"].default_value = (*rgb, OPAQUE)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


def water_material(name, rgb=WATER_COLOR, **properties):
    """Water shading in the runtime, with matching glass IOR in Blender's material preview.

    Overrides are material custom properties, exported as glTF extras. Use a subdivided,
    horizontal mesh; the runtime animates its vertices in world meters.
    """
    mat = material(name, rgb, roughness=WATER_ROUGHNESS)
    bsdf = mat.node_tree.nodes[PRINCIPLED_BSDF]
    bsdf.inputs["IOR"].default_value = WATER_IOR
    bsdf.inputs["Transmission Weight"].default_value = OPAQUE
    for key, value in (WATER_PROPERTIES | properties).items():
        mat[key] = value
    return mat


def _finish(obj, name, mat, props, parent):
    obj.name = name
    if obj.data is not None and hasattr(obj.data, "name"):
        obj.data.name = name
    if mat is not None:
        obj.data.materials.append(mat)
    for key, value in (props or {}).items():
        obj[key] = value
    if parent is not None:
        obj.parent = parent
    return obj


def apply_transform(obj, location=False, rotation=True, scale=True):
    """Bakes rotation/scale into the mesh so the object starts with a clean transform."""
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=location, rotation=rotation, scale=scale)


def box(name, location, dimensions, *, mat=None, props=None, parent=None, bake_scale=False):
    bpy.ops.mesh.primitive_cube_add(size=CUBE_SIZE, location=location)
    obj = bpy.context.active_object
    obj.scale = dimensions
    if bake_scale:
        apply_transform(obj)
    return _finish(obj, name, mat, props, parent)


def cylinder(name, location, radius, depth, *, mat=None, props=None, parent=None, rotation=NO_ROTATION, bake=True):
    bpy.ops.mesh.primitive_cylinder_add(vertices=DEFAULT_SEGMENTS, radius=radius, depth=depth,
                                        location=location, rotation=rotation)
    obj = bpy.context.active_object
    if bake:
        apply_transform(obj)
    return _finish(obj, name, mat, props, parent)


def sphere(name, location, radius, *, mat=None, props=None, parent=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=DEFAULT_SEGMENTS, ring_count=DEFAULT_RING_COUNT,
                                         radius=radius, location=location)
    obj = bpy.context.active_object
    bpy.ops.object.shade_smooth()
    return _finish(obj, name, mat, props, parent)


def empty(name, location, *, props=None, parent=None):
    obj = bpy.data.objects.new(name, None)
    obj.location = location
    bpy.context.collection.objects.link(obj)
    return _finish(obj, name, None, props, parent)


def look_rotation(source, target):
    direction = Vector(target) - Vector(source)
    return direction.to_track_quat(LOOK_AXIS, UP_AXIS).to_euler()


def camera(name, location, look_at, props=None):
    data = bpy.data.cameras.new(name)
    data.lens_unit = "FOV"
    data.angle = math.radians(CAMERA_FOV_DEGREES)
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    obj.rotation_euler = look_rotation(location, look_at)
    bpy.context.collection.objects.link(obj)
    bpy.context.scene.camera = obj
    return _finish(obj, name, None, props, None)


def sun(name, location, look_at, strength=SUN_STRENGTH):
    data = bpy.data.lights.new(name, type="SUN")
    data.energy = strength
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    obj.rotation_euler = look_rotation(location, look_at)
    bpy.context.collection.objects.link(obj)
    return obj


def point_light(name, location, rgb, watts=POINT_LIGHT_WATTS):
    data = bpy.data.lights.new(name, type="POINT")
    data.energy = watts
    data.color = rgb
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    bpy.context.collection.objects.link(obj)
    return obj


def export_glb(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        export_extras=True,
        export_cameras=True,
        export_lights=True,
        export_yup=True,
        export_apply=True,
        export_import_convert_lighting_mode="RAW",
    )
    print(f"[scene_kit] exported {path}")
