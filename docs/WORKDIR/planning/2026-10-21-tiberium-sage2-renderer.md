# Tiberium Project: the SAGE 2 (C&C4 / RA3) look in the Zero Hour engine

> [!NOTE]
> **AI-Generated Content Disclosure**: This document was written by an AI coding agent.

## Goal

Draw the game the way C&C4 Tiberian Twilight and Red Alert 3 draw it, with their own shaders and
art, on the Zero Hour engine: units, structures, terrain, water, trees, effects and the post
processing. Gameplay stays Zero Hour's (deterministic, replay-safe); only drawing changes.

The hand-written approximations added first (`Sage2MaterialClass`, `DamageLookClass`,
`PostProcessClass`) stay as the fallback and get replaced piece by piece by the real effects.

## What C&C4 ships

`Data/Shaders.big` holds 125 compiled Direct3D 9 effects (`fx_2_0`, mostly `vs_3_0` / `ps_3_0`
with `_M` / `_L` quality techniques in 2.0), among them:

- objects: `cnc4_object`, `_gdi`, `_nod`, `_alien`, `_*_proceduraldamage`, `_infantry`,
  `_tread*`, `_emissive`, `_flash*`, `_stealth*`, `_mirrorplating`, `_tiberium`, `_prop`, `_tree`
- scene: `deferredlighting`, `shadow`, `terraintile`, `terrainroad`, `terraincliff`,
  `terrainscorch`, `terraintracks`, `cnc4_water`, `ocean*`, `riverwater`, `puddle`, `rain`,
  `tree`, `treesway`, `decal`, `shroudeffect`
- effects: `gpuparticle*`, `cpuparticle`, `laser*`, `lightning`, `trail`, `tracer`,
  `muzzleflash`, `fxbeamhit`, `distortingobject`, `swarmparticle`
- post: `postfx_bloom`, `postfx_lookuptable`, `postfx_distortion`, `postfx_lineardepth`,
  `postfx_msaa`

Every effect follows the same contract (DirectX SAS): parameters carry `SasBindAddress`
annotations naming the engine value they want (`WW3D.*`, `Sas.*`, `Terrain.*`, `PostEffect.*`,
`Objects.*`), `unmanaged` parameters are the per-draw globals (`World`, `ViewProjection`,
`EyePosition`, `DirectionalLight_n_*`, `AmbientLightColor`, `Time`, `RecolorColor`, ...), and
the rest are the material's own constants from the model (`DiffuseTexture`, `NormalMap`,
`SpecMap`, `DamagedTexture`, `EnvMult`, `HouseColorUnitType`, `AlphaTestEnable`, ...).
Passes pick their shaders from arrays by a small CPU-side pre-shader (e.g. `PS_H_Array[index]`
with the index computed from `HasRecolorColors`, `StealthEnabled`, `HasShadow`,
`NumPointLights`, ...).

Lighting is deferred: a depth / normal pre-pass (every object effect has a
`_CreateDepthNormalMap` technique), `deferredlighting.fxo` turns it into screen-space diffuse
and specular light buffers (`WW3D.DeferredLighting.DiffuseLightTexture` / `SpecularLightTexture`),
and the object shaders read those at the pixel's screen position. Shadows come from a shadow
map (`shadow.fxo`, `Sas.Shadow[0].ShadowMap`).

## Architecture

```
Core/Libraries/Source/WWVegas/WW3D2/sage2/
  sage2effect.*      fx_2_0 loader: parameters, annotations, techniques, passes, states,
                     shaders (CTAB), pre-shaders (FXLC interpreter); Begin/End a pass with the
                     current parameter values: shader selection, constants, samplers, states
  sage2globals.*     the engine-side value table the SasBindAddress names resolve to
  sage2material.*    (replaces the hand-written one) draws a W3D mesh that carries a SAGE 2
                     material: loads the effect, feeds material constants + globals, extra
                     vertex stream (tangent, binormal, blend indices), picks the technique
  sage2scene.*       per-frame scene work: depth/normal pre-pass, shadow map, deferred light
                     buffers, post FX chain (bloom, look-up table)
```

The effects are loaded from the player's own C&C4 / RA3 install (shipped in the mod .big by the
extractor like the art); nothing of EA's is committed.

The W3D files get one new chunk per mesh, `W3D_CHUNK_SAGE2_MATERIAL` (text: effect name,
technique, constants, texture names). W3D readers that don't know it skip it.

## Phases

1. **Effect runtime** (in progress): loader + pre-shader interpreter + CTAB binding, checked
   offline (DXVK native on llvmpipe) by drawing the Crawler with `cnc4_object_gdi.fxo`.
2. **Deferred lighting**: depth/normal pre-pass and `deferredlighting.fxo` light buffers, in the
   harness, then in the engine's frame; shadow map after.
3. **Engine integration**: SAGE 2 material chunk, converter writes it, extra vertex stream,
   globals fed from W3D's lights / camera / time / house colour, objects draw with their real
   effects (procedural damage, treads, infantry, emissive, stealth, tiberium ...).
4. **Post FX**: `postfx_bloom` and `postfx_lookuptable` with C&C4's own look-up textures.
5. **Terrain, water, trees, particles, decals, shroud** with the C&C4 effects; the Zero Hour
   map data has to be fed to them (terrain tiles → atlas, water planes, tree models).
6. **RA3**: same runtime, RA3's effects and bindings (`objectsallied*`, `ocean`, ...) and RA3
   mods' C&C4-style materials.
7. **Older SAGE art (C&C3 / KW / ZH)**: convert their materials to the SAGE 2 effects
   (normal / spec maps generated where missing) so everything renders through one pipeline.

## Done so far (reference)

- `tools/fxo_parse.py`: the effect format reader (Python), used to study the effects.
- Findings: every pass sets its shaders through `*_Array[...]` + a standalone pre-shader;
  pass render states use the effect state table indices (0 ZEnable, 3 ZWriteEnable,
  6 SrcBlend, 7 DestBlend, 8 CullMode, 9 ZFunc, 10 AlphaRef, 11 AlphaFunc, 13 AlphaBlendEnable
  ...; 146 VertexShader, 147 PixelShader); sampler state ids are offset by 4 from MojoShader's
  names (4 Texture, 5 AddressU ... 9 MagFilter, 10 MinFilter, 11 MipFilter, 14 MaxAnisotropy,
  15 SRGBTexture).
