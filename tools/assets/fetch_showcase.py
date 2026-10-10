"""Downloads the CC0 assets of the photoreal showcase into .deps/assets/showcase (not committed).

    python3 -I tools/assets/fetch_showcase.py

Sources (all CC0, no attribution required):
  Poly Haven  https://polyhaven.com   (HDRIs, scanned models, textures)
  ambientCG   https://ambientcg.com   (scanned road / facade materials)
"""

import io
import json
import os
import sys
import urllib.request
import zipfile

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEST = os.path.join(REPO_ROOT, ".deps", "assets", "showcase")
# Poly Haven refuses Python's default user agent.
HEADERS = {"User-Agent": "redgem-asset-fetch/1.0"}
POLYHAVEN_API = "https://api.polyhaven.com/files/{}"
HDRI_URL = "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/{res}/{id}_{res}.hdr"
AMBIENTCG_URL = "https://ambientcg.com/get?file={id}_{res}-JPG.zip"
# Poly Haven API keys of the PBR maps: color, OpenGL normal, AO/roughness/metal.
TEXTURE_MAPS = ("Diffuse", "nor_gl", "arm")
TIMEOUT_SECONDS = 300

HDRIS = {"belfast_sunset_puresky": "4k"}
MODELS = {  # Poly Haven model id -> glTF resolution
    "street_lamp_01": "2k",
    "fire_hydrant": "1k",
    "metal_trash_can": "1k",
    "island_tree_02": "1k",
    "planter_box_01": "1k",
    "modular_street_seating": "1k",
    "utility_box_01": "1k",
    "water_manhole_cover": "1k",
    "concrete_road_barrier": "1k",
    "jacaranda_tree": "1k",
    "outdoor_table_chair_set_01": "1k",
    "painted_wooden_bench": "1k",
    "potted_plant_01": "1k",
    "potted_plant_02": "1k",
    "potted_plant_04": "1k",
    "planter_box_02": "1k",
    "planter_box_03": "1k",
    "utility_box_02": "1k",
    "trashbag": "1k",
    "cardboard_box_01": "1k",
    "plastic_crate_02": "1k",
    "concrete_road_barrier_02": "1k",
}
POLYHAVEN_TEXTURES = {"concrete_pavement": "2k", "concrete_floor_worn_001": "2k"}
AMBIENTCG_MATERIALS = {"Road007": "2K", "Facade006": "2K", "Facade018A": "2K", "Facade020A": "2K"}


def fetch(url):
    request = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
        return response.read()


def save(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as file:
        file.write(data)


def fetch_hdri(asset_id, res):
    path = os.path.join(DEST, "hdri", f"{asset_id}_{res}.hdr")
    if not os.path.exists(path):
        save(path, fetch(HDRI_URL.format(id=asset_id, res=res)))
    return path


def fetch_model(asset_id, res):
    """Poly Haven glTF: the .gltf plus every file it includes, at their relative paths."""
    folder = os.path.join(DEST, "models", asset_id)
    files = json.loads(fetch(POLYHAVEN_API.format(asset_id)))
    entry = files["gltf"][res]["gltf"]
    gltf_path = os.path.join(folder, os.path.basename(entry["url"]))
    if not os.path.exists(gltf_path):
        for relative, include in entry.get("include", {}).items():
            save(os.path.join(folder, relative), fetch(include["url"]))
        save(gltf_path, fetch(entry["url"]))
    return gltf_path


def fetch_polyhaven_texture(asset_id, res):
    folder = os.path.join(DEST, "textures", asset_id)
    files = json.loads(fetch(POLYHAVEN_API.format(asset_id)))
    for map_name in TEXTURE_MAPS:
        options = files.get(map_name, {}).get(res, {})
        image = options.get("jpg") or options.get("png")
        if image is None:
            continue
        path = os.path.join(folder, os.path.basename(image["url"]))
        if not os.path.exists(path):
            save(path, fetch(image["url"]))


def fetch_ambientcg(asset_id, res):
    folder = os.path.join(DEST, "textures", asset_id)
    if os.path.isdir(folder) and os.listdir(folder):
        return
    with zipfile.ZipFile(io.BytesIO(fetch(AMBIENTCG_URL.format(id=asset_id, res=res)))) as archive:
        archive.extractall(folder)


def main():
    steps = ([(fetch_hdri, k, v) for k, v in HDRIS.items()] + [(fetch_model, k, v) for k, v in MODELS.items()] +
             [(fetch_polyhaven_texture, k, v) for k, v in POLYHAVEN_TEXTURES.items()] +
             [(fetch_ambientcg, k, v) for k, v in AMBIENTCG_MATERIALS.items()])
    for function, asset_id, res in steps:
        print(f"{function.__name__}: {asset_id} ({res})", flush=True)
        function(asset_id, res)
    print(f"assets ready in {DEST}")


if __name__ == "__main__":
    sys.exit(main())
