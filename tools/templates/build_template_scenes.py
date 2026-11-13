#!/usr/bin/env python3
"""Writes the scene.glb of the third-person, top-down and menu-only templates.

    python3 -I tools/templates/build_template_scenes.py

The shapes are the editor's own primitives (GameObject > 3D Object), as the empty and platformer
templates have them: the ground plane and the cube of templates/empty, the capsule of
templates/platformer. Each object gets its gameplay data (collider, body, script, tag) as node
extras, which the engine reads as it reads the editor's. The file is written as the editor writes
one: a mesh and a material per object, then the sun and the camera. Open the result in the editor
to change it: what it saves is the same kind of file.
"""

import json
import math
import os
import struct

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TEMPLATES = os.path.join(REPO_ROOT, "templates")

GLB_MAGIC = b"glTF"
GLB_VERSION = 2
JSON_CHUNK = b"JSON"
BIN_CHUNK = b"BIN\x00"
HEADER_BYTES = 12
CHUNK_HEADER_BYTES = 8
ALIGNMENT = 4
JSON_PADDING = b" "
BIN_PADDING = b"\x00"
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963
ATTRIBUTES = ("POSITION", "NORMAL", "TEXCOORD_0")

ROUGHNESS = 0.6
OPAQUE = 1.0
NO_ROTATION = [0.0, 0.0, 0.0, 1.0]
SUN_ROTATION = [-0.43602994084358215, 0.2579646706581116, 0.1320192515850067, 0.8519994616508484]
SUN_COLOR = [1.0, 0.96, 0.88]
SUN_INTENSITY = 3.0
CAMERA_YFOV = 0.87
CAMERA_ZNEAR = 0.1
CAMERA_ZFAR = 1000.0

# The primitives, by the template and the mesh that has them.
PLANE = ("empty", "Ground")
CUBE = ("empty", "Cube")
CAPSULE = ("platformer", "Player")

GROUND_COLOR = [0.32, 0.36, 0.34]
GROUND_SIZE = [3.0, 1.0, 3.0]      # the plane is 10 meters wide: 30 by 30
HERO_COLOR = [0.9, 0.3, 0.1]
HERO_HEIGHT = 1.2                  # the capsule's middle: its 2 meters stand on the ground
CRATE_COLOR = [0.62, 0.42, 0.22]
ROCK_COLOR = [0.45, 0.45, 0.5]
LEDGE_COLOR = [0.15, 0.35, 0.75]


def pitch_rotation(degrees):
    """A quaternion [x, y, z, w] that turns down (negative degrees) or up around X."""
    half = math.radians(degrees) / 2.0
    return [math.sin(half), 0.0, 0.0, math.cos(half)]


def read_glb(path):
    """The JSON and the binary chunk of a .glb file."""
    with open(path, "rb") as file:
        data = file.read()
    json_length = struct.unpack_from("<I", data, HEADER_BYTES)[0]
    json_start = HEADER_BYTES + CHUNK_HEADER_BYTES
    document = json.loads(data[json_start:json_start + json_length])
    bin_start = json_start + json_length + CHUNK_HEADER_BYTES
    return document, data[bin_start:]


def accessor_data(document, binary, index):
    """An accessor of a glTF file (without its buffer view) and the bytes it reads."""
    accessor = dict(document["accessors"][index])
    view = document["bufferViews"][accessor.pop("bufferView")]
    start = view.get("byteOffset", 0)
    return accessor, binary[start:start + view["byteLength"]]


def primitive_mesh(source):
    """The attributes and indices of a mesh of a template: {name: (accessor, bytes)}."""
    template, mesh_name = source
    document, binary = read_glb(os.path.join(TEMPLATES, template, "scene.glb"))
    mesh = next(mesh for mesh in document["meshes"] if mesh["name"] == mesh_name)
    primitive = mesh["primitives"][0]
    parts = {name: accessor_data(document, binary, primitive["attributes"][name]) for name in ATTRIBUTES}
    parts["indices"] = accessor_data(document, binary, primitive["indices"])
    return parts


def thing(name, shape, color, position, scale=(1.0, 1.0, 1.0), **extras):
    """An object of a scene: a primitive of a color, where it is, and its gameplay data."""
    return {"name": name, "shape": shape, "color": color, "position": list(position), "scale": list(scale),
            "extras": extras}


class GlbWriter:
    """A scene being written: its buffer and the glTF arrays that point into it."""

    def __init__(self):
        self.binary = bytearray()
        self.document = {"asset": {"generator": "RedGem", "version": "2.0"}, "accessors": [], "bufferViews": [],
                         "materials": [], "meshes": [], "nodes": []}
        self.primitives = {}

    def add_view(self, accessor, data, target):
        """Adds bytes to the buffer, and an accessor that reads them; returns the accessor's index."""
        self.binary.extend(BIN_PADDING * (-len(self.binary) % ALIGNMENT))
        views = self.document["bufferViews"]
        views.append({"buffer": 0, "byteLength": len(data), "byteOffset": len(self.binary), "target": target})
        self.binary.extend(data)
        self.document["accessors"].append({**accessor, "bufferView": len(views) - 1})
        return len(self.document["accessors"]) - 1

    def add_mesh(self, name, shape, color):
        """A mesh of its own for an object, with its own material; returns the mesh's index."""
        if shape not in self.primitives:
            self.primitives[shape] = primitive_mesh(shape)
        parts = self.primitives[shape]
        attributes = {key: self.add_view(*parts[key], ARRAY_BUFFER) for key in ATTRIBUTES}
        indices = self.add_view(*parts["indices"], ELEMENT_ARRAY_BUFFER)
        materials = self.document["materials"]
        materials.append({"emissiveFactor": [0.0, 0.0, 0.0], "name": name, "pbrMetallicRoughness": {
            "baseColorFactor": [*color, OPAQUE], "metallicFactor": 0.0, "roughnessFactor": ROUGHNESS}})
        primitive = {"attributes": attributes, "indices": indices, "material": len(materials) - 1}
        self.document["meshes"].append({"name": name, "primitives": [primitive]})
        return len(self.document["meshes"]) - 1

    def add_object(self, spec):
        mesh = self.add_mesh(spec["name"], spec["shape"], spec["color"])
        node = {"mesh": mesh, "name": spec["name"], "rotation": NO_ROTATION, "scale": spec["scale"],
                "translation": spec["position"]}
        if spec["extras"]:
            node["extras"] = spec["extras"]
        self.document["nodes"].append(node)

    def add_sun_and_camera(self, camera_position, camera_pitch, camera_extras):
        self.document["extensionsUsed"] = ["KHR_lights_punctual"]
        self.document["extensions"] = {"KHR_lights_punctual": {"lights": [
            {"color": SUN_COLOR, "intensity": SUN_INTENSITY, "name": "Sun", "type": "directional"}]}}
        self.document["cameras"] = [{"name": "Camera", "type": "perspective", "perspective": {
            "yfov": CAMERA_YFOV, "zfar": CAMERA_ZFAR, "znear": CAMERA_ZNEAR}}]
        nodes = self.document["nodes"]
        nodes.append({"extensions": {"KHR_lights_punctual": {"light": 0}}, "name": "Sun", "rotation": SUN_ROTATION,
                      "scale": [1.0, 1.0, 1.0], "translation": [0.0, 0.0, 0.0]})
        camera = {"camera": 0, "name": "Camera", "rotation": pitch_rotation(camera_pitch), "scale": [1.0, 1.0, 1.0],
                  "translation": list(camera_position)}
        if camera_extras:
            camera["extras"] = camera_extras
        nodes.append(camera)

    def write(self, path):
        self.document["buffers"] = [{"byteLength": len(self.binary)}]
        self.document["scene"] = 0
        self.document["scenes"] = [{"name": "Scene", "nodes": list(range(len(self.document["nodes"])))}]
        text = json.dumps(self.document, separators=(",", ":")).encode()
        text += JSON_PADDING * (-len(text) % ALIGNMENT)
        binary = bytes(self.binary) + BIN_PADDING * (-len(self.binary) % ALIGNMENT)
        total = HEADER_BYTES + 2 * CHUNK_HEADER_BYTES + len(text) + len(binary)
        with open(path, "wb") as file:
            file.write(struct.pack("<4sII", GLB_MAGIC, GLB_VERSION, total))
            file.write(struct.pack("<I4s", len(text), JSON_CHUNK) + text)
            file.write(struct.pack("<I4s", len(binary), BIN_CHUNK) + binary)


def ground():
    return thing("Ground", PLANE, GROUND_COLOR, (0.0, 0.0, 0.0), GROUND_SIZE, collider="box")


def hero(script):
    return thing("Player", CAPSULE, HERO_COLOR, (0.0, HERO_HEIGHT, 0.0), body="character", collider="capsule",
                 script=script, tag="player")


def third_person():
    """A hero on open ground, with crates to walk around and a ledge to jump onto."""
    return [ground(), hero("player.rb"),
            thing("Crate1", CUBE, CRATE_COLOR, (6.0, 1.0, -2.0), (2.0, 2.0, 2.0), collider="box"),
            thing("Crate2", CUBE, CRATE_COLOR, (-3.0, 0.5, -4.0), collider="box"),
            thing("Ledge", CUBE, LEDGE_COLOR, (0.0, 0.5, -12.0), (4.0, 1.0, 4.0), collider="box")]


def top_down():
    """A hero on open ground, seen from above, with rocks to walk around."""
    return [ground(), hero("player.rb"),
            thing("Rock1", CUBE, ROCK_COLOR, (9.0, 0.75, -4.0), (1.5, 1.5, 1.5), collider="box"),
            thing("Rock2", CUBE, ROCK_COLOR, (-4.0, 0.5, 4.0), collider="box"),
            thing("Rock3", CUBE, ROCK_COLOR, (-6.0, 1.0, -7.0), (2.0, 2.0, 2.0), collider="box")]


def menu_only():
    """Only the ground, behind the menus."""
    return [ground()]


# Each template's objects, and where its camera starts: position, pitch (degrees), extras.
SCENES = {
    "third-person": (third_person, (0.0, 4.0, 8.0), -15.0, {"script": "orbit_camera.rb"}),
    "top-down": (top_down, (0.0, 15.2, 9.0), -57.0, {"script": "top_down_camera.rb"}),
    "menu-only": (menu_only, (0.0, 4.0, 8.0), -23.6, {}),
}


def main():
    for template, (objects, camera_position, camera_pitch, camera_extras) in SCENES.items():
        writer = GlbWriter()
        for spec in objects():
            writer.add_object(spec)
        writer.add_sun_and_camera(camera_position, camera_pitch, camera_extras)
        path = os.path.join(TEMPLATES, template, "scene.glb")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        writer.write(path)
        print(f"wrote {os.path.relpath(path, REPO_ROOT)} ({os.path.getsize(path)} bytes)")


if __name__ == "__main__":
    main()
