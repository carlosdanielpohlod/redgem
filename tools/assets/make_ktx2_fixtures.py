#!/usr/bin/env python3
"""Makes the small KTX2 (Basis Universal) pictures the tests read: one ETC1S picture without alpha
and one UASTC picture with alpha, both with their mipmaps. Their pixels are drawn here; the
official basisu tool (https://github.com/BinomialLLC/basis_universal, which the engine does not
ship: it carries only the transcoder) compresses them.

    python3 -I tools/assets/make_ktx2_fixtures.py --basisu "basisu"

--basisu is the command that runs the tool, split on spaces (its WebAssembly build run by a WASI
runtime works too).
"""
import argparse
import os
import shlex
import struct
import subprocess
import tempfile
import zlib

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT_DIR = os.path.join(REPO_ROOT, "tests", "fixtures", "images")
OPAQUE = 255
CLEAR = 0
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
RGBA_8_BITS = (8, 6, 0, 0, 0)  # bit depth, color type RGBA, compression, filter, interlace
NO_FILTER = b"\x00"
RED = (OPAQUE, CLEAR, CLEAR)
BLUE = (CLEAR, CLEAR, OPAQUE)
WHITE = (OPAQUE, OPAQUE, OPAQUE)
# (file name, width, height, basisu options). The opaque one is wider than tall, so its smallest
# mipmaps are narrower than a compressed block.
OPAQUE_PICTURE = ("etc1s_opaque", 16, 8, ["-mipmap"])
ALPHA_PICTURE = ("uastc_alpha", 8, 8, ["-uastc", "-mipmap"])


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def write_png(path, width, height, pixel_at):
    """pixel_at(x, y) gives the (r, g, b, a) of a pixel."""
    rows = []
    for y in range(height):
        rows.append(NO_FILTER + b"".join(bytes(pixel_at(x, y)) for x in range(width)))
    with open(path, "wb") as out:
        out.write(PNG_SIGNATURE)
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, *RGBA_8_BITS)))
        out.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
        out.write(chunk(b"IEND", b""))


def halves(width):
    """Red on the left half and blue on the right, opaque."""
    return lambda x, _y: (*(RED if x < width // 2 else BLUE), OPAQUE)


def see_through_bottom(height):
    """White, opaque on the top half and fully see-through on the bottom."""
    return lambda _x, y: (*WHITE, OPAQUE if y < height // 2 else CLEAR)


def compress(basisu, folder, picture, pixel_at):
    name, width, height, options = picture
    png = name + ".png"
    write_png(os.path.join(folder, png), width, height, pixel_at)
    output = os.path.join(OUTPUT_DIR, name + ".ktx2")
    command = [*basisu, "-ktx2", *options, "-file", png, "-output_file", name + ".ktx2"]
    subprocess.run(command, cwd=folder, check=True, stdout=subprocess.DEVNULL)
    os.replace(os.path.join(folder, name + ".ktx2"), output)
    print("wrote", os.path.relpath(output, REPO_ROOT))


def main():
    parser = argparse.ArgumentParser(description="Writes the KTX2 test pictures.")
    parser.add_argument("--basisu", default="basisu", help="the command that runs the basisu tool")
    basisu = shlex.split(parser.parse_args().basisu)
    with tempfile.TemporaryDirectory() as folder:
        compress(basisu, folder, OPAQUE_PICTURE, halves(OPAQUE_PICTURE[1]))
        compress(basisu, folder, ALPHA_PICTURE, see_through_bottom(ALPHA_PICTURE[2]))


if __name__ == "__main__":
    main()
