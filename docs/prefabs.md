# Prefab files

A scene can refer to a reusable object stored in `prefabs/crate.glb` inside the game
folder. Keep exactly one root in that file. On an empty scene object, set the string
custom property `prefab_file` to `prefabs/crate.glb`. Paths are relative to the game
folder, including references inside another prefab. Symbolic links and paths outside
the game are refused.

Loading replaces the empty object's content with the prefab root's content. The
placement keeps its name, identity and parent. Its transform combines with the
source root's transform. Its custom properties override the source root's properties;
its existing children remain. Descendants come from the file. Geometry, materials
and images are shared by repeated instances of the same file.

A source root marked `prefab = true` becomes live when placed. Mark the reference
itself `prefab = true` to keep an inert template for `Scene.spawn` instead. Each scene
load reads the current files; changing a prefab does not update a running scene.

Descendant identities are deterministic for the source file path, placement identity
and source node identity. Renaming a source node preserves an explicit identity;
renaming the prefab file changes the generated descendant identities. Root placement
identities remain unchanged.

Animate descendants rather than the source root. Imported clips are named
`prefabs/crate.glb::Idle`, for example. Repeated instances share that clip's name;
per-object animation playback still affects only that object's hierarchy. Animated
placements require an identity transform on the source root.

Cycles and oversized expansions fail the load before replacing the running scene.
Defaults allow 64 source files, eight nested references, 4,096 instances and 100,000
expanded nodes, with 256 MiB of source GLB files and 512 MiB of estimated expanded
content. The GLB reader also checks a tighter memory budget before reading buffers. Editor
previews count copied scene data against their budget before allocating the snapshot.

The editing viewport previews inherited geometry, lights and cameras. Clicking inherited parts
selects the authored reference; its placement pivot stays independent of source-root offsets.
Moving references, copying, reparenting and undo preserve their world placement. Saving keeps
references in the scene file, with inherited assets in their original prefab files.

Reopening or reloading the scene rereads its prefab sources. Placement drags use cached source
data. Missing or invalid references report an error and leave the authored scene editable so
the link can be repaired. Playing still validates every dependency.

The Project panel's **Prefabs** folder lists `prefabs/*.glb`. Double-click a file to place a
linked instance in front of the editor camera, drag it onto the Scene view to place it at the
ground under the pointer, or onto a Hierarchy object to parent it there. Placement validates
the complete scene's prefab dependencies first. A failed placement leaves the scene, selection
and undo history unchanged. Repeated instances share source assets and have independent identities.

Creating prefab files and overriding inherited children remain future work; create source
files in your modeling tool for now.
Built games include regular files in `prefabs/` along with the other game assets.
