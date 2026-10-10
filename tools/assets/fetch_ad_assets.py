"""Downloads the car and the people of the advertising render into .deps/assets/ad (not committed).

    python3 -I tools/assets/fetch_ad_assets.py

Source: BlenderKit (https://www.blenderkit.com), free assets only. Their licenses allow commercial
renders (advertising included) without attribution, but not redistributing the models, so they
stay out of the repository. The script refuses any asset whose license is not one of those.
Chosen for advertising: an unbranded car (fictional logo) and people with no brand logos on
their clothes.
"""

import json
import os
import urllib.parse
import urllib.request

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEST = os.path.join(REPO_ROOT, ".deps", "assets", "ad")
SEARCH_URL = "https://www.blenderkit.com/api/v1/search/?query={}"
# Download URLs answer with a signed file URL; an anonymous scene id is enough for free assets.
ANONYMOUS_SCENE = "00000000-0000-0000-0000-000000000000"
HEADERS = {"User-Agent": "Mozilla/5.0 (X11; Linux x86_64) redgem-asset-fetch/1.0"}
ALLOWED_LICENSES = ("royalty_free", "cc_zero")
TIMEOUT_SECONDS = 600

# folder -> BlenderKit asset base id
ASSETS = {
    "generic_sport_sedan": "d3d94329-838b-46c3-9cc1-1e07c4c7b705",
    "andrew": "390a2d40-740f-4224-95fb-2a3583391889",
    "wenceslavus": "bb4ba046-b1a9-4ed8-b7f8-2c9b34d7dae8",
    "luke": "102eb4d4-c0dc-4c9e-acf2-5787ca1e06c4",
    "charles": "9a70afa3-2457-486c-94d4-aeedd36b4b72",
    "melissa_white": "45a50b10-510c-42c4-bdf0-2f280bac48ba",
    "melissa_green": "aad9b212-d927-468a-8a00-c08081a7eea2",
    "axel": "5b716b0c-52df-4847-bfda-1b95de5db765",
}


def get_json(url):
    request = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
        return json.loads(response.read())


def download(url, path):
    request = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response, open(path, "wb") as out:
        out.write(response.read())


def fetch(folder, base_id):
    target = os.path.join(DEST, folder, folder + ".blend")
    if os.path.exists(target):
        print(f"[ad] {folder}: already downloaded")
        return
    query = urllib.parse.quote(f"asset_base_id:{base_id}")
    results = get_json(SEARCH_URL.format(query)).get("results", [])
    if not results:
        raise SystemExit(f"[ad] {folder}: asset {base_id} not found")
    asset = results[0]
    if asset.get("license") not in ALLOWED_LICENSES or not asset.get("isFree"):
        raise SystemExit(f"[ad] {folder}: license {asset.get('license')} is not allowed")
    blend = next(f for f in asset["files"] if f.get("fileType") == "blend")
    signed = get_json(f"{blend['downloadUrl']}?scene_uuid={ANONYMOUS_SCENE}")["filePath"]
    os.makedirs(os.path.dirname(target), exist_ok=True)
    download(signed, target)
    with open(os.path.join(DEST, folder, "blenderkit_search.json"), "w") as out:
        json.dump(asset, out, indent=1)  # name, author and license, for the credits
    print(f"[ad] {folder}: {asset['name']} by {asset['author'].get('fullName')} ({asset['license']})")


def main():
    for folder, base_id in ASSETS.items():
        fetch(folder, base_id)


if __name__ == "__main__":
    main()
