# Tiberium Project: renderer options

The C&C4-style renderer on the `tiberium/d3d9-renderer` branch reads these lines from `Options.ini`
in the user data folder (`Documents\Command and Conquer Generals Zero Hour Data` on Windows). Every
line is optional; the value shown is the default. `yes`/`no` switches also take `1`/`0`. They are read
when the game starts (the display's `W3DDisplay::init`), so a change needs a restart.

Most switches only matter with `CNC4Style = yes`; with `no` the game draws as stock Zero Hour (the
reference for mods), apart from the Direct3D 9 renderer underneath.

## The renderer

| Line | Default | What it does |
| --- | --- | --- |
| `CNC4Style` | `yes` | The C&C4 / RA3 look: C&C4 lighting by map theme, the post chain, the terrain, shadow map, water, roads, SSAO and soft particles below. `no`: Zero Hour's own look. |
| `Sage2Look` | `ra3` | Whose light sets the maps that are not snowy get: `ra3` (Red Alert 3's light and saturating table) or `cnc4` (C&C4's Pan-America). Snow maps always take C&C4's Arctic. |
| `Sage2Lighting` | `auto` | `auto` picks the preset from each map's theme and time of day. A preset name forces one: `temperate`, `evening`, `arctic`, `arctic_day`, `junktown`, `junktown_day`, `night`, `ra3_day`, `ra3_morning`, `ra3_evening`, `ra3_night`. |
| `Sage2MapMood` | `0` | How much of each map's own mood (how dim its light is, and its colour) the preset keeps, 0 to 1. |
| `Sage2NightLight` | `0.5` | The moon's strength on maps that play at night. |
| `Sage2NightLightSnow` | `0.25` | The same on snowy night maps (snow reflects most of it). |
| `Sage2NightTint` | `0` | How much of the map's own night colour the moon takes, 0 to 1. |
| `Sage2LinearLights` | `no` | Hand the light colours to the effects in linear light instead of as the maps store them. |
| `Sage2Deferred` | `yes` | C&C4's deferred lighting (the terrain and C&C4 models lit from light buffers). |
| `Sage2ObjectShaders` | `forward` | Zero Hour's own plain meshes through C&C4's BasicW3D effect: `forward` (lit like the ground, under its shadow map), `deferred`, or `off` (W3D's fixed-function draw). |
| `Sage2FixedFunctionShaders` | `yes` on Windows' own Direct3D 9, `no` on DXVK | What W3D still draws without shaders goes through shaders that do the same, so no draw of a frame is left to the fixed-function pipeline (groundwork for a modern backend). Off on DXVK, where it costs about 2.6 ms a frame. |
| `Sage2EffectCheck` | `no` | Load every compiled effect the game and its mods carry (`Art\Shaders\*.fxo`) at start and write each result to `Sage2.log`: a check of the effect reader. |

The Direct3D 9 implementation is chosen with the environment variable `GX_D3D9_BACKEND`: `dxvk` (the
default: DXVK next to the game, on Vulkan), `native` (Windows' own, on the graphics driver) or `d3d12`
(Windows' own, through its D3D9On12 layer).

## Post chain

| Line | Default | What it does |
| --- | --- | --- |
| `Sage2Post` | `yes` | C&C4's bloom, tone map and colour grading table. |
| `Sage2Exposure` | `1.0` | A scale on the exposure. |
| `Sage2AutoExposure` | `0.5` | How far each map's exposure follows its ground's brightness, 0 to 1 (bright snow is brought down). |
| `Sage2LookupStrength` | `1.0` | The grading table: 0 none, 1 as the look sets it. |
| `Sage2KeepBlack` | `yes` | Black past the map's edge stays black under the grade. |
| `Sage2AntiAliasing` | `yes` | Edge anti-aliasing (FXAA) after the grade. |
| `Sage2AntiAliasingSubpixel` | `0.25` | Its sub-pixel smoothing. |
| `Sage2AntiAliasingThreshold` | `0.166` | Its edge threshold. |
| `PostProcess` | `yes` | Post-processing on the world, under the interface, at all: `no` turns off the C&C4 chain too. With it on and `Sage2Post = no` (or C&C4's post effects missing from the mod), the project's simpler bloom and grade run instead. |

## Terrain, roads, water

| Line | Default | What it does |
| --- | --- | --- |
| `Sage2Terrain` | `yes` | The ground through the C&C4-style terrain effect. |
| `Sage2TerrainAlbedo` | `0.6` under a preset, `1.6` under the maps' own lights | A lift on Zero Hour's tiles, roads and tracks in linear light. |
| `Sage2TerrainTileSize` | `80` | World units per C&C4 terrain texture. |
| `Sage2TerrainBumpFlip` | `no` | Flip the normal maps' green channel. |
| `Sage2TerrainBumpScale` | `0.75` | Bump strength. |
| `Sage2TerrainFillSpecular` | `0` with the RA3 look, `1` with C&C4's | The fill lights' highlight on the ground. |
| `Sage2TerrainCrossBlend` | `yes` | Patch edges between natural terrains blended across the cell borders. |
| `Sage2TerrainCrossNoise` | `0.5` | How far a soft noise moves those edges. |
| `Sage2TerrainCrossSoftness` | `0.25` | Their softness. |
| `Sage2TerrainMacro` | `tsnoiseurb` | The ground's large-scale noise texture. |
| `Sage2TopView` | `yes` | Scorch marks baked into the ground, C&C4's way. |
| `Sage2TopViewSize` | `2048` | That texture's size. |
| `Sage2Roads` | `yes` | Roads lit as the terrain. |
| `Sage2Water` | `yes` | C&C4/RA3-style lakes and rivers. |
| `Sage2WaterDepthScale` | `2.5` | Depth scale for RA3's depth table under the surface. |
| `Sage2WaterTint` | `-1` | How much the water's colour tints what is under it, 0 to 1; negative: 0.3 with RA3's depth table, 0.4 without. |
| `Sage2WaterCaustics` | `0.6` | Caustics strength. |
| `Sage2WaterSky` | `0.6` | The sky reflected (by the Fresnel term). |
| `Sage2WaterShallow` | `20` | Depth in world units over which shallow water is tinted. |

## Shadows and ambient occlusion

| Line | Default | What it does |
| --- | --- | --- |
| `Sage2Shadows` | `yes` | C&C4's shadow map in place of Zero Hour's shadows. |
| `Sage2ShadowMapSize` | `2048` | Its size. |
| `Sage2ShadowSoft` | `1.0` | C&C4's soft kernel step, in texels. |
| `Sage2ShadowJitter` | `0.5` | Its jitter, in texels. |
| `Sage2ShadowDebug` | `no` | Show the shadow map in the top-left corner. |
| `Sage2SSAO` | `yes` | Screen-space ambient occlusion (the project's own; C&C4 shipped without). |
| `Sage2SSAORadius` | `12` | How far around a point the occluders count, in world units. |
| `Sage2SSAOIntensity` | `1.0` | How dark a closed-off point gets. |
| `Sage2SSAOStrength` | `0.8` | How much of it goes onto the frame. |
| `Sage2SSAODebug` | `no` | Show the occlusion alone, in grey. |

## Lights and particles

| Line | Default | What it does |
| --- | --- | --- |
| `Sage2NightLights` | `yes` | Buildings' lit windows glow at night. |
| `Sage2PointLights` | `2.0` | The strength of the point lights on the ground: fires, lit windows, street lamps, headlights (0: none). |
| `Sage2PulseLights` | `1.0` | The strength of the explosions' and muzzle flashes' light pulses, which fade from their centre as Zero Hour's did. |
| `Sage2LitParticles` | `yes` | Smoke and dust lit by the scene and by nearby fires. |
| `Sage2SoftParticles` | `yes` | Particles fade where they meet the ground, units and buildings. |
| `Sage2SoftParticleFade` | `10` | The shortest distance they fade over, in world units; larger particles fade over their own radius. |
| `Sage2TreeLight` | `yes` | Trees lit in linear light, as the effects light the rest. |
| `HeatEffects` | (Zero Hour's) | Zero Hour's heat haze, which C&C4's heat distortions are converted to. |

Zero Hour's own lines (`Resolution`, `UseShadowVolumes`, `DynamicLOD` and the rest) work as before.
`DynamicLOD = yes` drops particles when frames get slow.
