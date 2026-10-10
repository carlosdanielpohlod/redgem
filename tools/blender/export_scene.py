"""Exports an edited .blend file to the scene.glb the engine loads, with the right settings.

    blender -b games/platformer/platformer.blend --python tools/blender/export_scene.py -- games/platformer/scene.glb
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scene_kit as kit  # noqa: E402

kit.export_glb(kit.output_path_from_argv("scene.glb"))
