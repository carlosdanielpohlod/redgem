# Coastal water validation

Photorealistic acceptance remains open. Geometric refraction controls pass, but the coastal
study still shows overly regular distant reflections and large procedural foam patches.
The current filtered-normal roughness approximation needs further validation against integrated
shading. Passing geometric and temporal controls does not establish a photographic result.

## Matched optical study

Both views use the same scanned coast, camera, sun, sky, seven-component wave field and
0.5-second clock at 1600 by 900 pixels. Foam is disabled in both optical views. Exposure is
0.8 with the same display transform; there is no alignment or exposure fitting.
The reference uses Cycles with 32 samples, denoising, IOR 1.333, roughness 0.07 and the same
absorption coefficients. Its isotropic volume scattering is a physical approximation of the
runtime body-light term; atmospheric fog and derivative filtering do not have identical models.
The micro-height field is represented by bump shading in the reference.

| Runtime | Cycles reference |
| --- | --- |
| ![Runtime optical study](images/water-coastal/engine.png) | ![Cycles optical study](images/water-coastal/reference.png) |

The finite scan ends above a separate deeper substrate. Both pictures show that geometric
edge; it must be distinguished from duplicated or displaced refraction samples.

Fixed rectangles exclude the shore and sky. Coordinates are `[left, top, right, bottom]` in
original pixels. MAE is the mean absolute RGB byte difference. SSIM uses an 11-pixel Gaussian
window with sigma 1.5, population covariance, constants 0.01 and 0.03, and a five-pixel border
removed from the result. Values below are computed from the lossless pictures above.

| Region | Rectangle | RGB MAE | RGB SSIM |
| --- | --- | ---: | ---: |
| Distant water | [40, 260, 1560, 325] | 13.797 | 0.3061 |
| Foreground water | [200, 740, 1350, 880] | 28.880 | 0.4300 |

A seven-wave preview without the new unresolved micro-normal variance measured 12.073 MAE /
0.4081 SSIM in the distant rectangle. The current approximation regresses that comparison;
its reflection filtering remains under investigation before visual acceptance.

## Correctness controls

- Isolate screen-space reflection from transmission using submerged and exposed emissive objects.
- Compare transmitted silhouettes with an independent CPU Snell-law intersection, including
  supported interior coverage and false copies outside the postprocess footprint.
- Compare absorption outside a partially exposed object's silhouette against the identical
  unobstructed flat or inclined floor.
- Reproduce a missed inclined-depth root when search and refinement use different samplers.
- Require a soft, irregular foam band, open water pockets and partial bottom transmission.
- Check displacement bounds, analytic gradients, stationary clocks and filtered normal energy.

The full render suite passed 76 cases and 655593 assertions. Unit checks passed 1215 cases
and 42068 assertions. Local shader-validation binaries rebuild shader construction and changed
tests while reusing unchanged CPU libraries. Clean builds remain a CI requirement.
The actual GLES shader pairs for lighting, motion, depth, particle shadows and temporal resolve
all compiled and linked on a native GLES context. This does not replace an Android package build.

Negative controls remove micro-normal variance or restore the continuous foam coverage floor:
they respectively lose energy at 4608 distant probe pixels and produce no open pockets in
1089 shallow-shelf samples. Restoring nearest-depth bracketing makes the inclined-root probe
return the wrong sample coordinate. The corrected versions pass those checks.

## Cost

RTX 4050 Laptop GPU, 1600 by 900, clock 0.5, 60 warmup frames and 600 measured frames per run.
One process runs at a time. Order is baseline, candidate, candidate, baseline.
These are render-only frame times; physical interaction detection and game logic are excluded.

| Version | Run medians (ms) | Mean of medians (ms) |
| --- | --- | ---: |
| Main baseline | 9.812, 9.898 | 9.855 |
| Water changes | 11.443, 11.520 | 11.482 |

This scene costs about 1.63 ms more, or 16.5 percent. The measurement includes the changed
wave field, normal filtering, refraction and procedural foam together.

## Assets

The local study uses [Coast Rocks 01](https://polyhaven.com/a/coast_rocks_01), by Rico Cilliers
and Rob Tuytel, and [Ballito Rocky Beach](https://polyhaven.com/a/ballito_rocky_beach), from
Poly Haven under CC0. Only rendered validation pictures are included here; source assets and
the demonstration project remain local.
