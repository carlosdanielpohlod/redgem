# Rendering regression validation

These synthetic scenes isolate temporal detail and opening-edge rejection. They are renderer
regressions, not a claim of photographic scene quality. The images come from the native renderer.

## Fast camera

A tracked subject travels with the camera while a striped roadside wall crosses the screen at
more than 32 pixels per frame. The camera stays below the global camera-cut threshold. The
reference uses the same final view and jitter phase without accumulated history.

| Previous resolve | Updated resolve | Fresh reference |
| --- | --- | --- |
| ![Previous chase](images/temporal-validation/chase-before.png) | ![Updated chase](images/temporal-validation/chase-after.png) | ![Fresh chase](images/temporal-validation/chase-reference.png) |

The mean sum of absolute RGB byte differences per pixel in the wall region was 41.1073 with
the previous shader. The updated resolve meets the regression threshold of 0.5, with both zero
and four MSAA samples. Temporal antialiasing disabled produces the same final pixels as a fresh
view. Existing static and slowly moving foliage tests still pass.

## Opening edge

A foreground card moves about 7.7 pixels while its background moves about 1.6 pixels. Both
remain below the fast-motion threshold. The regression selects 51 freshly revealed black pixels
next to the card. Their mean sum of absolute RGB byte errors falls from 588 to zero.

| Previous resolve | Updated resolve |
| --- | --- |
| ![Previous opening edge](images/temporal-validation/edge-before.png) | ![Updated opening edge](images/temporal-validation/edge-after.png) |

The previous shader fails both new regressions. History rejection uses current motion and its
neighbor disagreement; it still does not validate history against previous-frame depth. Fine
moving detail can trade smear for shimmer.

## Particle shadows and water refactoring

Particle render tests distinguish receiver darkening from the visible sprite, compare circular
and rectangular silhouettes, check transparent flipbook cells and lifetime alpha, clear expired
casters, and require identical disabled frames before and after an enabled frame.

The water shader is divided into small helpers for intersection, reflection, lighting,
transmission, foam and composition. Each function body is at most 13 lines. All 28 water
regression images remain pixel-identical across this refactoring. Across existing non-temporal
scenes, 83 shared screenshots remain pixel-identical to the earlier renderer.

## Validation environment

On a GeForce RTX 4050 Laptop GPU, the combined change passed 1,193 C++ unit/integration cases
(41,877 assertions), 23 Ruby suites and 52 render cases (655,490 assertions). Include-direction
checks pass. The lit, depth, motion, particle-shadow and temporal programs also compile and link
as GLSL ES 3.00 through native Mesa Intel EGL. This is not Android-device validation.

With 60 warm-up frames and 600 measured frames at 1600 x 900, without vertical synchronization,
the scatter field measured 3.107 ms average / 3.106 ms median and the local showcase scene
5.091 ms average / 5.075 ms median. These are measurements of this run, not a controlled claim
of speedup over another revision. Showcase assets are local and are not included here.
