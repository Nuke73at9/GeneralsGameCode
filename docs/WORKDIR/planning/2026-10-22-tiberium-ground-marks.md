# Tiberium Project: roads, scorch marks, tracks and decals — C&C4's method against Zero Hour's

> [!NOTE]
> **AI-Generated Content Disclosure**: This document was written by an AI coding agent from the C&C4 modding-support shader sources (`Terrain.fxh`, `TerrainRoad.fx`, `TerrainScorch.fx`, `TerrainTracks.fx`, `Decal.fx`, `InGame.scrape`, `TerrainTopView.scrapeh`) and Zero Hour's renderer sources, before any of it was implemented.

Direction (22/10): *"See what you can find in CNC4 and compare it amongst each handling, because there is a method we want to make sure it's correct."* This is that comparison, with what the Tiberium renderer will do for each.

## The frame, in C&C4's order

`InGame.scrape` draws the ground in this order, every piece over the last:

1. terrain (`renderTerrain`)
2. roads (`renderRoads`)
3. "terrain-like ground objects": cliff pieces, sea walls, road segments as models (`renderbin TerrainLikeGroundObject`)
4. terrain tracks (`renderTerrainTracks`)
5. bridges, then props
6. units, structures, the rest of the solid objects
7. water (reflection and refraction already made before the frame)
8. decals (`renderDecals`)
9. transparent objects, particles

Scorch marks are **not in this list**: they are painted into a texture before the frame and the terrain, roads and cliffs read that texture. That is the method worth getting right.

## 1. Scorch marks

**C&C4.** `TerrainTopView.scrapeh`: a top-down orthographic camera over the map renders every scorch quad (and the deploy-zone effect) into a map-covering RGBA texture, `TerrainTopViewTexture`, cleared to transparent. `TerrainScorch.fx` draws each scorch with the terrain's own vertex/pixel code (`Terrain.fxh` with `TERRAIN_SCORCH`), flattens `Position.z` to 0 (the top view has no depth), takes the scorch texture's alpha as opacity, converts the colour to linear light and divides it by 4 (`TerrainTopViewColorRange`, headroom so bright marks can bloom). No lighting happens there.

Then every ground shader (`Terrain.fxh` for tiles, roads and cliffs) samples that texture at the pixel's world xy (`CalcTerrainTopViewTexCoord`: `(xy - offset) * scale`, v flipped) and bakes it into the albedo **before lighting**:

```
baseColor = baseColor * (1 - scorch.a) + scorch.rgb * 4
specFactor = 1 - scorch.a          // no specular on a burn
```

So a scorch is lit, shadowed and bumped exactly like the ground under it, it never fights the terrain's depth, it costs one texture read in the terrain shader rather than overdraw per mark, and it appears on roads and cliffs too.

**Zero Hour.** `W3DScorch`: a quad per scorch, one vertex per terrain cell it covers, placed `MAP_HEIGHT_SCALE/10` (0.06 units) above the ground, textured from the `EXScorch` atlas (6 marks a row, each 1.5 cells apart in the atlas), vertex colour from the terrain lighting, drawn with `_PresetAlphaShader` (alpha blend, depth test, no depth write) after roads and props. Lighting is whatever was baked into the vertex colour; with the terrain effect lit in linear light, these marks would sit on top unlit by the sun's shadows and flicker where the quad and the ground share a depth.

**Tiberium.** C&C4's way: `Sage2TopViewClass` keeps a map-sized RGBA texture (2048², about 2 units a texel on a 400x400 map; made at map load, redrawn only when a scorch is added or the map changes), draws `W3DScorch`'s quads into it with a `_TopView` technique of the terrain effect (z flattened, alpha from the atlas, linear colour ÷ 4), and `tiberium_terrain.fx` gets the two lines above with `ScorchMarkUVOffsetMultiplier`. `drawScorches()` then draws nothing when the top view exists. Roads get the same read.

## 2. Roads

**C&C4.** Roads are strips on the ground with the terrain's vertex layout (position, colour, uv) drawn by `TerrainRoad.fx` = `Terrain.fxh` with `TERRAIN_ROAD`: the same deferred pre-pass as the terrain (the road's normal and depth into the buffers, tangent frame `cross(N,-X)`, `cross(N,+Y)`), then the lit pass from the light buffers with the road's **own colour and normal map** (`BaseSamplerWrapped`, `NormalSamplerWrapped`), alpha-blended edges (`SrcAlpha`), depth-tested against the terrain with `DepthBias -0.0004` and **no depth write**, and the scorch texture read at the world position. Roads are therefore bumped, specular, shadowed and burned like the ground.

**Zero Hour.** `W3DRoadBuffer`: strips whose vertices sit on the highest corner of their cell (`getMaxCellHeight`) plus `MAP_HEIGHT_SCALE/8` (0.08 units), vertex colour from the terrain lighting, fixed-function `ST_ROAD_BASE` (texture x colour, with the cloud and noise layers), alpha-blended, depth-tested, no depth write. No normal map; the lift above the ground is how it avoids fighting the terrain.

**Tiberium.** A `tiberium_road.fx` built from C&C4's `TerrainRoad.fx`: `_CreateDepthNormalMap` + `Default` (deferred) and `Default_M` (forward), the road's normal = the height field's normal at the vertex (ZH's road vertices carry none; the side stream the terrain already keeps supplies it), bias through `D3DRS_DEPTHBIAS` as C&C4's `-0.0004`, scorch read as above. ZH's road textures first (flat normal where there is no `_nrm`); C&C4's road textures are not in `Terrain.big` (its roads are largely models in `TerrainLikeGroundObject`), so a C&C4 road set is a sourcing job for later, as the terrain textures were.

## 3. Tracks (vehicle treads)

**C&C4.** `TerrainTracks.fx`: strips with position, uv and a per-vertex colour that carries the fade; texture `WW3D.MiscTexture` clamped; **unlit** — the pixel shader is `color * GammaToLinear(texture)` (so the mark's darkness is in linear light, matching the lit ground around it), alpha-blended, depth-tested, no depth write, drawn right after the roads and ground objects (before bridges, props and units). No bias: the strips are placed by the engine.

**Zero Hour.** `TerrainTracksRenderObjClassSystem`: the same idea — edge pairs laid along the vehicle's path, 2 units (`0.2 * MAP_XY_FACTOR`) above the ground, vertex alpha fading with age and distance, `_PresetAlphaShader`, flushed after the bridges.

**Tiberium.** Draw ZH's track buffer with C&C4's `terraintracks.fxo` as it ships (`Default`: linear-light multiply). Nothing else changes: the strips, the fade and the 2-unit lift are ZH's and fine. One line in `W3DTerrainTracks::flush`.

## 4. Decals and projected marks

**C&C4.** `Decal.fx`: a quad or mesh with its own world transform, techniques `Alpha`, `Add`, `Multiply` (and stencil-merge variants), `DepthBias -0.0001`, no depth write, drawn after the water (`renderDecals`). Used for selection, placement, deploy zones (`CNC4_DeployZone.fx` is its own effect), beacons. Object shadows are **not** decals in C&C4: the shadow map (done 22/10) covers them.

**Zero Hour.** `W3DProjectedShadowManager` (`TexProjectClass`): a texture is projected through the terrain tiles under the object (the tiles are drawn again with a projective texture matrix) and onto any object its box reaches; used for the shadow decals of structures and trees, and for a few decal-like effects. Building foundations are `W3DBibBuffer` quads. Selection and placement rings are the game's own drawables.

**Tiberium.** With the shadow map on, ZH's shadow decals are already off (`W3DScene::Flush`, `W3DTreeBuffer`). Nothing else of ZH's projected pass is needed for the C&C4 look; `decal.fxo` is there for the placement/deploy effects when those are redone (item 6/7 work), through the SAGE 2 mesh path with its `Alpha` / `Add` / `Multiply` techniques.

## 5. Cliffs

**C&C4.** Cliff faces are models (`TerrainCliff.fx`, `CNC4_Terrain_Object.fx`) lit like the terrain, drawn in the `TerrainLikeGroundObject` bin over the terrain.

**Zero Hour.** Cliffs are height-map cells with cliff texture classes; already drawn by the terrain effect with C&C4's cliff textures (22/10).

**Tiberium.** Stays as it is; a model-cliff pass would be new map content, not a renderer matter.

## Order of work

1. Scorch top view (the method that differs most and the one that makes the others consistent).
2. Roads through the terrain-family effect, reading the top view.
3. Tracks through `terraintracks.fxo`.
4. Three-way blends and shorelines folded into the terrain effect (from the 22/10 shroud fix) — same family, same pass, so they take the top view and the lighting too.
