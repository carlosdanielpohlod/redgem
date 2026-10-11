#!/usr/bin/env python3
"""Draws the pictures the demo games put on their particles: a puff of smoke and a spark. Both are
white with see-through edges, so a game tints them with the particle's color. Drawn by this
script, so there is nobody to credit.

    python3 -I tools/assets/make_particle_images.py
"""
import math
import os
import random
import struct
import zlib

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
# Which games get which pictures.
OUTPUTS = {
    "smoke": [os.path.join("games", "showcase", "images")],
    "spark": [os.path.join("games", "showcase", "images"), os.path.join("games", "platformer", "images")],
}
SIZE = 128                 # pixels across
OPAQUE = 255
WHITE = 255
SEED = 11
# The puff: a few soft blobs around the middle.
BLOBS = 9
BLOB_SPREAD = 0.42         # how far from the middle a blob may sit, as a share of the half size
BLOB_RADIUS = (0.38, 0.62) # of the half size
PUFF_DENSITY = 0.75        # how solid the middle of the puff is
# The spark: a bright core with four rays.
CORE_RADIUS = 0.16         # of the half size
RAY_THINNESS = 26.0        # higher makes thinner rays
RAY_REACH = 2.2            # how fast a ray fades toward the edge
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
RGBA_8_BITS = (8, 6, 0, 0, 0)   # bit depth, color type RGBA, compression, filter, interlace
NO_FILTER = b"\x00"


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def write_png(path, alpha_at):
    """alpha_at(x, y) with x and y from -1 to 1 gives how solid the white picture is there, 0 to 1."""
    rows = []
    for row in range(SIZE):
        pixels = bytearray()
        for column in range(SIZE):
            x = (column + 0.5) / SIZE * 2.0 - 1.0
            y = (row + 0.5) / SIZE * 2.0 - 1.0
            alpha = int(round(max(0.0, min(1.0, alpha_at(x, y))) * OPAQUE))
            pixels += bytes((WHITE, WHITE, WHITE, alpha))
        rows.append(NO_FILTER + bytes(pixels))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as out:
        out.write(PNG_SIGNATURE)
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, *RGBA_8_BITS)))
        out.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
        out.write(chunk(b"IEND", b""))


def smooth(edge_from, edge_to, value):
    t = max(0.0, min(1.0, (value - edge_from) / (edge_to - edge_from)))
    return t * t * (3.0 - 2.0 * t)


def smoke():
    rng = random.Random(SEED)
    blobs = []
    for _ in range(BLOBS):
        angle = rng.uniform(0.0, math.tau)
        distance = rng.uniform(0.0, BLOB_SPREAD)
        blobs.append((math.cos(angle) * distance, math.sin(angle) * distance, rng.uniform(*BLOB_RADIUS)))

    def alpha_at(x, y):
        clear = 1.0
        for blob_x, blob_y, radius in blobs:
            clear *= 1.0 - (1.0 - smooth(0.0, radius, math.hypot(x - blob_x, y - blob_y))) * 0.5
        # Nothing reaches the picture's edge, or the square would show.
        return (1.0 - clear) * PUFF_DENSITY * (1.0 - smooth(0.6, 1.0, math.hypot(x, y)))

    return alpha_at


def spark():
    def alpha_at(x, y):
        distance = math.hypot(x, y)
        core = 1.0 - smooth(0.0, CORE_RADIUS * 2.0, distance)
        rays = max(math.exp(-RAY_THINNESS * abs(x)), math.exp(-RAY_THINNESS * abs(y))) * math.exp(-RAY_REACH * distance)
        return max(core, rays) * (1.0 - smooth(0.75, 1.0, distance))

    return alpha_at


def main():
    shapes = {"smoke": smoke(), "spark": spark()}
    for name, folders in OUTPUTS.items():
        for folder in folders:
            path = os.path.join(REPO_ROOT, folder, name + ".png")
            write_png(path, shapes[name])
            print("wrote", os.path.relpath(path, REPO_ROOT))


if __name__ == "__main__":
    main()
