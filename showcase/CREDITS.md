# Showcase credits

The renders in `showcase/renders` may be used commercially, advertising included. No attribution
is required; sources are listed for reference.

## Street (CC0 1.0, public domain)

- HDRI `belfast_sunset_puresky`, scanned models (street lamp, island tree, jacaranda tree, fire
  hydrant, trash can, street seating, utility boxes, planters, potted plant, manhole cover, road
  barrier, outdoor table and chairs, wooden bench, trash bag, cardboard box, plastic crate) and textures
  (`concrete_pavement`, `concrete_floor_worn_001`) by **Poly Haven**: https://polyhaven.com
- Materials `Road007`, `Facade006`, `Facade018A`, `Facade020A` by **ambientCG**: https://ambientcg.com

## Car and people (BlenderKit, Royalty Free license)

BlenderKit's Royalty Free license allows commercial use of renders without credit. The models
themselves may not be redistributed or resold, so they are not in this repository
(`tools/assets/fetch_ad_assets.py` downloads them into `.deps/assets/ad`).

- **Generic Sport Sedan 2022 PL** by Pierre-Louis Baril: designed after no existing car, with a
  fictional logo and plate, so it shows no real car brand.
- **Andrew**, **Wenceslavus**, **Charles** and **Luke** (scanned people) by Kyb3r Project. They were
  chosen because their clothes show no brand logos.
- **Standing Cute Girl White Jacket Jeans**, **Standing Survivor Girl Green Top Cowboy Boots** and
  **Axel Rainbow Collar Shirt Standing** (scanned people) by Calvin Serrano: only in the playable
  street, also without brand logos.

Licenses: https://www.blenderkit.com/docs/licenses/

The real-time export (`games/showcase/scene.glb`, not committed) and any standalone build of it
contain those models: do not publish or share them, only images and videos made with them.

## Rebuild

```
python3 -I tools/assets/fetch_showcase.py
python3 -I tools/assets/fetch_ad_assets.py
tools/blender/run.sh build_showcase.py showcase
```

This renders with Cycles on an NVIDIA RTX card (OptiX) and then exports the playable scene to
`games/showcase`. `SHOWCASE_PREVIEW=1` gives a quick preview, `SHOWCASE_SHOT=ad|hero|street` picks
the camera, `SHOWCASE_RENDER=0` skips the render and `SHOWCASE_EXPORT=0` the export. The export
needs about 4.5 GB of memory: close the editor first (`BLENDER_MEMORY_MAX=6500M` makes Blender
stop instead of freezing the machine if it runs out).
