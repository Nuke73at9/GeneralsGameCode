# Tiberium Project: terrain patch edges that cross cell borders (plan)

> [!NOTE]
> **AI-Generated Content Disclosure**: This plan was written by an AI coding agent from the 24/10/2026 investigation (see the worklog).

## The problem

Where Zero Hour's terrain tiles are replaced by C&C4's textures (`tiberium_terrain.fx`, the atlas from
`Data\TerrainCNC4.txt`), the edges between patches show the cell grid: stair steps one cell (10 world
units) wide, and some straight hard lines along a cell's side. Seen on Golden Oasis (the user's PC),
El Scorcho and Dust Devil (Linux).

Zero Hour's blend tiles turn one terrain into the next *within* a cell (along its sides and corners),
and some neighbouring cells change terrain with no blend tile at all. Between Zero Hour's similar
browns and olives neither shows; between C&C4's orange cracked earth, pale sand and grey gravel both do.

Tried and not kept: moving the edge with a soft noise inside each cell's own blend. The edges turn
ragged, but the one-cell steps and the hard sides stay, because an edge cannot leave the cell that
blends. The shader part, applied to `zhBlend.w * vertexAlpha` in `SampleGround` (and to the
`ExtraBlend` alpha), with `BlendNoise` = (amount 1, 1 / 14 world units, softness 0.35):

```hlsl
float NoisyBlend(float w, float2 worldXY)
{
	float2 p = worldXY * BlendNoise.y;
	float n = (0.65 * BlendValueNoise(p) + 0.35 * BlendValueNoise(p * 2.7 + 17.3)) * 2.0 - 1.0;
	float middle = 4.0 * w * (1.0 - w);		// 1 halfway through the blend, 0 where the cell is one terrain
	w = saturate(w + n * BlendNoise.x * middle);
	return smoothstep(0.5 - BlendNoise.z, 0.5 + BlendNoise.z, w);
}
```

(`BlendValueNoise`: a smoothed value noise from a `frac(sin(dot(p, (127.1, 311.7))) * 43758.5453)` hash.)

## The idea

Let an edge wander across the cell border. Each pixel knows its own cell's terrains (base and blend
slot, already in the side stream) and, new, its four neighbours' visible terrain near the shared side:

1. **Data.** `WorldHeightMap::getCNC4Slots(x, y, &base, &blend)` already gives a cell's slots. In
   `HeightMapRenderObjClass::updateVB` (Core `HeightMap.cpp`, `STORE_SIDE_NORMAL`), also store the
   neighbours' *dominant* slot along each side: for the cell to the left, its blend slot if its blend
   tile covers its right side (alpha at that side > 0.5), else its base slot; the same for the right,
   top and bottom. `SIDE_FLOATS` grows from 5 to 9 (`Sage2TerrainClass`, the vertex declaration's
   second stream, `Update_Normals`); corners can use the side neighbours.
   An alternative with less vertex data: a map-sized slot texture (one texel a cell, like the shroud's,
   `R8G8` base and blend), point-sampled by the pixel shader at the neighbour cells' centres.
2. **Shader.** In `SampleGround`, from the pixel's position within its cell (`frac(worldPos.xy / 10)`)
   take the distance to each side. Within a band of about a third of a cell, blend towards the
   neighbour's terrain with a weight that is 0.5 at the side itself (the neighbour's pixel at the same
   point blends back by the same rule, so the two sides meet without a seam), shaped by a world-space
   noise so the edge wanders. Only where the neighbour's terrain differs from this pixel's own.
3. **Blend tiles inside the cell** stay as they are; the noise-moved edge (the experiment) can come
   back on top of this for their interior part.
4. **Checks.** Golden Oasis, El Scorcho, Dust Devil, Barren Badlands close up (the all-maps camera
   views), the render check (`alpine_town` has blends; references to store again), and the PC
   (`tiberium_gpu_check_ab.bat` with an option to turn it off, `Sage2TerrainCrossBlend`).

## Risks

- Cliffs and roads: cliff cells use their own tiles (triplanar); skip cells whose neighbour is a cliff
  tile class, or the rock would bleed onto flat ground.
- The three-way blend quads (`ExtraBlend`) are drawn over the finished ground; they keep Zero Hour's
  shapes, so a third terrain may still show a cell edge.
- Water edges and the map border: neighbours outside the drawn area take the cell's own slot.
