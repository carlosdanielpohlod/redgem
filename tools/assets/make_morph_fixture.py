"""Writes tests/fixtures/morph.glb: a one-meter square with two shape keys and a clip that moves them.

    python3 tools/assets/make_morph_fixture.py tests/fixtures/morph.glb

The square ("Face", facing +Z) has the shape keys "Smile" (its top edge half a meter up, its
normals turned toward +Y there) and "Wide" (its right edge half a meter out), starting at the
weights 0.25 and 0. The clip "Talk" takes them from (0, 0) to (1, 0.5) in one second. Plain Python:
no Blender needed.
"""

import json
import struct
import sys

HALF = 0.5
SMILE_LIFT = 0.5
WIDE_STRETCH = 0.5
NORMAL_TURN = 0.25
DEFAULT_WEIGHTS = [0.25, 0.0]
TALK_TIMES = [0.0, 1.0]
TALK_WEIGHTS = [0.0, 0.0, 1.0, 0.5]  # one weight per shape key, per key of the clip

FLOAT = 5126
UNSIGNED_SHORT = 5123
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963
GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942
GLB_HEADER_BYTES = 12
AXES = 3
ARGUMENTS = 2  # the script and the file it writes
ALIGNMENT = 4

POSITIONS = [(-HALF, -HALF, 0.0), (HALF, -HALF, 0.0), (HALF, HALF, 0.0), (-HALF, HALF, 0.0)]
NORMALS = [(0.0, 0.0, 1.0)] * len(POSITIONS)
INDICES = [0, 1, 2, 0, 2, 3]
NO_DELTA = (0.0, 0.0, 0.0)
SMILE_POSITIONS = [NO_DELTA, NO_DELTA, (0.0, SMILE_LIFT, 0.0), (0.0, SMILE_LIFT, 0.0)]
SMILE_NORMALS = [NO_DELTA, NO_DELTA, (0.0, NORMAL_TURN, 0.0), (0.0, NORMAL_TURN, 0.0)]
WIDE_POSITIONS = [NO_DELTA, (WIDE_STRETCH, 0.0, 0.0), (WIDE_STRETCH, 0.0, 0.0), NO_DELTA]


class Buffer:
    """The binary chunk, with a buffer view and an accessor for each list of numbers put in it."""

    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def add(self, values, kind, component=FLOAT, target=None):
        """Adds `values` (tuples for VEC3, numbers for SCALAR) and returns the accessor's index."""
        flat = [v for value in values for v in value] if kind == "VEC3" else list(values)
        code = f"<{len(flat)}{'f' if component == FLOAT else 'H'}"
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": struct.calcsize(code)}
        if target is not None:
            view["target"] = target
        self.data += struct.pack(code, *flat)
        self.data += b"\0" * (-len(self.data) % ALIGNMENT)
        self.views.append(view)
        accessor = {"bufferView": len(self.views) - 1, "componentType": component, "count": len(values), "type": kind}
        if kind == "VEC3":
            accessor["min"] = [min(value[axis] for value in values) for axis in range(AXES)]
            accessor["max"] = [max(value[axis] for value in values) for axis in range(AXES)]
        self.accessors.append(accessor)
        return len(self.accessors) - 1


def mesh_and_clip(buffer):
    position = buffer.add(POSITIONS, "VEC3", target=ARRAY_BUFFER)
    normal = buffer.add(NORMALS, "VEC3", target=ARRAY_BUFFER)
    indices = buffer.add(INDICES, "SCALAR", UNSIGNED_SHORT, ELEMENT_ARRAY_BUFFER)
    smile = {"POSITION": buffer.add(SMILE_POSITIONS, "VEC3"), "NORMAL": buffer.add(SMILE_NORMALS, "VEC3")}
    wide = {"POSITION": buffer.add(WIDE_POSITIONS, "VEC3")}
    primitive = {"attributes": {"POSITION": position, "NORMAL": normal}, "indices": indices, "targets": [smile, wide]}
    mesh = {
        "name": "Face",
        "primitives": [primitive],
        "weights": DEFAULT_WEIGHTS,
        "extras": {"targetNames": ["Smile", "Wide"]},
    }
    sampler = {
        "input": buffer.add(TALK_TIMES, "SCALAR"),
        "output": buffer.add(TALK_WEIGHTS, "SCALAR"),
        "interpolation": "LINEAR",
    }
    clip = {
        "name": "Talk",
        "samplers": [sampler],
        "channels": [{"sampler": 0, "target": {"node": 0, "path": "weights"}}],
    }
    return mesh, clip


def document():
    buffer = Buffer()
    mesh, clip = mesh_and_clip(buffer)
    gltf = {
        "asset": {"version": "2.0", "generator": "RedGem make_morph_fixture.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Face", "mesh": 0}],
        "meshes": [mesh],
        "animations": [clip],
        "accessors": buffer.accessors,
        "bufferViews": buffer.views,
        "buffers": [{"byteLength": len(buffer.data)}],
    }
    return gltf, bytes(buffer.data)


def chunk(kind, payload, pad):
    payload += pad * (-len(payload) % ALIGNMENT)
    return struct.pack("<II", len(payload), kind) + payload


def glb_bytes():
    gltf, binary = document()
    body = chunk(JSON_CHUNK, json.dumps(gltf, separators=(",", ":")).encode(), b" ") + chunk(BIN_CHUNK, binary, b"\0")
    return struct.pack("<III", GLB_MAGIC, GLB_VERSION, GLB_HEADER_BYTES + len(body)) + body


def main():
    if len(sys.argv) != ARGUMENTS:
        sys.exit(__doc__)
    with open(sys.argv[1], "wb") as out:
        out.write(glb_bytes())


if __name__ == "__main__":
    main()
