#!/usr/bin/env python3
"""Writes THIRD_PARTY_LICENSES.md: the license of every project whose code or fonts end up inside
the engine and inside the games built with it. Run it after a build (the dependencies' sources,
with their license files, are in build/_deps) whenever a dependency is added or updated.

    python3 -I tools/licenses/collect_notices.py
    python3 -I tools/licenses/collect_notices.py --deps .deps/fetch --check

--deps names where the build keeps the dependencies' sources (FETCHCONTENT_BASE_DIR) when that is
not build/_deps. --check writes nothing: it fails when the file in the repository is not what the
sources make, which is how the continuous integration catches a dependency updated without it.
"""
import argparse
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
# Where the license files are looked for: the dependencies' sources, kept by the build. The
# projects below name their files with this placeholder, replaced by --deps (or build/_deps).
DEPS = "{deps}"
DEFAULT_DEPS = os.path.join(REPO_ROOT, "build", "_deps")
OUTPUT = os.path.join(REPO_ROOT, "THIRD_PARTY_LICENSES.md")
# What the check exits with when the file is stale (1 is any other failure of Python's own).
EXIT_STALE = 2

# (name, what it is used for, where it comes from, its license file).
PROJECTS = [
    ("raylib", "Window, input, OpenGL rendering, audio", "https://www.raylib.com",
     os.path.join(DEPS, "raylib-src", "LICENSE")),
    ("Jolt Physics", "Physics", "https://github.com/jrouwe/JoltPhysics",
     os.path.join(DEPS, "joltphysics-src", "LICENSE")),
    ("mruby", "The Ruby interpreter that runs game scripts", "https://mruby.org",
     os.path.join(DEPS, "mruby-src", "LICENSE")),
    ("cgltf", "Loading glTF scenes", "https://github.com/jkuhlmann/cgltf",
     os.path.join(DEPS, "cgltf-src", "LICENSE")),
    ("nlohmann/json", "JSON, and MessagePack for multiplayer messages", "https://github.com/nlohmann/json",
     os.path.join(DEPS, "nlohmann_json-src", "LICENSE.MIT")),
    ("Mbed TLS", "HTTPS for the games' web requests (HTTP)", "https://www.trustedfirmware.org/projects/mbed-tls/",
     os.path.join(DEPS, "mbedtls-src", "LICENSE")),
    # (In the public domain: its "license" is the blessing its sources start with.)
    ("SQLite", "Where a dedicated server keeps its game's saves", "https://www.sqlite.org",
     os.path.join(DEPS, "sqlite-src", "sqlite3.h")),
    ("Dear ImGui", "The editor's interface", "https://github.com/ocornut/imgui",
     os.path.join(DEPS, "imgui-src", "LICENSE.txt")),
    ("rlImGui", "Dear ImGui on raylib", "https://github.com/raylib-extras/rlImGui",
     os.path.join(DEPS, "rlimgui-src", "LICENSE")),
    ("ImGuizmo", "The editor's move, rotate and scale gizmos", "https://github.com/CedricGuillemet/ImGuizmo",
     os.path.join(DEPS, "imguizmo-src", "LICENSE")),
    ("Font Awesome Free", "The editor's icons", "https://fontawesome.com",
     os.path.join(DEPS, "rlimgui-src", "extras", "FontAwsome_LICENSE.txt")),
    ("libwebp", "Decoding static WebP images", "https://developers.google.com/speed/webp",
     os.path.join(DEPS, "libwebp-src", "COPYING")),
    ("libwebp patent grant", "Additional rights for the WebP implementation",
     "https://developers.google.com/speed/webp", os.path.join(DEPS, "libwebp-src", "PATENTS")),
    ("FreeType", "Decoding game-provided fonts; copyright © 1996–2026 The FreeType Project, "
     "all rights reserved (FreeType License)", "https://freetype.org",
     os.path.join(DEPS, "freetype-src", "docs", "FTL.TXT")),
    ("Inter", "The editor's and the games' text font", "https://rsms.me/inter/",
     os.path.join(REPO_ROOT, "assets", "editor", "fonts", "Inter-LICENSE.txt")),
    ("JetBrains Mono", "The editor's code font", "https://www.jetbrains.com/lp/mono/",
     os.path.join(REPO_ROOT, "assets", "editor", "fonts", "JetBrainsMono-LICENSE.txt")),
]

# Libraries that come inside the ones above or with the compilers, and what they are licensed under.
BUNDLED = [
    ("GLFW (inside raylib)", "zlib/libpng", "https://www.glfw.org/license.html"),
    ("stb_image, stb_image_write, stb_image_resize2, stb_truetype, stb_rect_pack, stb_vorbis, stb_perlin "
     "(inside raylib)", "MIT or public domain", "https://github.com/nothings/stb"),
    ("miniaudio (inside raylib)", "MIT No Attribution or public domain", "https://miniaud.io"),
    ("dr_wav, dr_mp3, dr_flac (inside raylib)", "MIT No Attribution or public domain",
     "https://github.com/mackron/dr_libs"),
    ("glad (inside raylib)", "MIT; the generated OpenGL headers under Apache 2.0",
     "https://github.com/Dav1dde/glad"),
    ("sdefl, sinfl (inside raylib)", "MIT or public domain", "https://github.com/vurtun/lib"),
    ("qoa, qoi (inside raylib)", "MIT", "https://github.com/phoboslab"),
    ("jar_xm, jar_mod (inside raylib)", "WTFPL / public domain", "https://github.com/kd7tck/jar"),
    ("par_shapes, msf_gif, rprand, rl_gputex, m3d, tinyobj_loader_c, vox_loader (inside raylib; "
     "compiled in, not used by the engine)", "MIT or zlib", "https://github.com/raysan5/raylib/tree/master/src/external"),
    ("LLVM libc++ and compiler-rt (linked into Windows and Android builds)",
     "Apache 2.0 with LLVM exceptions", "https://llvm.org/LICENSE.txt"),
    ("mingw-w64 runtime (linked into Windows builds)", "Zope Public License 2.1 and public domain",
     "https://www.mingw-w64.org"),
    ("Everest and p256-m (inside Mbed TLS: elliptic curves)", "Apache 2.0",
     "https://github.com/Mbed-TLS/mbedtls/tree/development/3rdparty"),
    ("Android native app glue (compiled into Android builds)", "Apache 2.0",
     "https://developer.android.com/ndk"),
]

HEADER = """# Third-party licenses

RedGem is released under the MIT license (see [LICENSE](LICENSE)). It is built on the projects
below; their code or fonts end up inside the engine and inside the games built with it, under
their own licenses. This file is generated by `tools/licenses/collect_notices.py`.

A game built with RedGem contains the engine. If you distribute one, pass this file along with it.

Tools used to make games but not included in them (Blender, the Android SDK and NDK, CMake) keep
their own licenses and are not covered here.
"""


def license_text(path):
    """A license file's text; for a C header, the comment it starts with, up to its first line of stars."""
    with open(path, encoding="utf-8") as text:
        content = text.read()
    if not path.endswith(".h"):
        return content.strip()
    lines = []
    for line in content.splitlines()[1:]:
        if line.startswith("*****"):
            break
        lines.append(line.removeprefix("**").strip())
    return "\n".join(lines).strip()


def notices(deps):
    """The whole text of THIRD_PARTY_LICENSES.md, from the license files under deps."""
    parts = [HEADER, "\n## Projects\n"]
    for name, purpose, url, _ in PROJECTS:
        parts.append(f"- **{name}**: {purpose}. {url}")
    parts.append("\n## Inside those, or linked in by the compilers\n")
    for name, license_name, url in BUNDLED:
        parts.append(f"- **{name}**: {license_name}. {url}")
    parts.append("\n## License texts\n")
    for name, _, _, license_file in PROJECTS:
        parts.append(f"### {name}\n\n```\n{license_text(license_file.replace(DEPS, deps))}\n```\n")
    return "\n".join(parts)


def check(text, path=OUTPUT):
    """Exits with EXIT_STALE when the committed file (path) differs from text."""
    with open(path, encoding="utf-8") as committed:
        if committed.read() == text:
            print(os.path.basename(path), "is up to date")
            return
    print(os.path.basename(path), "is stale: run tools/licenses/collect_notices.py and commit it",
          file=sys.stderr)
    sys.exit(EXIT_STALE)


def main():
    parser = argparse.ArgumentParser(description="Writes (or checks) THIRD_PARTY_LICENSES.md.")
    parser.add_argument("--deps", default=DEFAULT_DEPS, help="where the dependencies' sources are")
    parser.add_argument("--check", action="store_true", help="fail if the file is stale instead of writing it")
    args = parser.parse_args()
    text = notices(os.path.abspath(args.deps))
    if args.check:
        check(text)
        return
    with open(OUTPUT, "w", encoding="utf-8") as out:
        out.write(text)
    print("wrote", os.path.relpath(OUTPUT, REPO_ROOT))


if __name__ == "__main__":
    main()
