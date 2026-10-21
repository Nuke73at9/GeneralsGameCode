# Tiberium Project: Direct3D 9 renderer

> [!NOTE]
> **AI-Generated Content Disclosure**: This document was written by an AI coding agent.

## Goal

Draw C&C4 Tiberian Twilight's look in the Zero Hour engine: its procedural damage, normal and
specular maps, house colour. C&C4 is a Direct3D 9 game (shader model 3); the W3D renderer is
Direct3D 8, whose shader models (vs 1.1 / ps 1.4) cannot run those shaders.

## Decision: Direct3D 9 through DXVK, not Direct3D 11

- C&C4's shaders are Direct3D 9 effects (`fx_2_0`, `vs_3_0` / `ps_3_0`), so they port almost
  one-to-one.
- DXVK already ships Direct3D 9 (it is what its Direct3D 8 runs on), so drawing still ends up on
  Vulkan, on Windows, Linux and macOS alike.
- The engine talks to Direct3D directly in about 50 files and through `DX8Wrapper` everywhere else;
  most Direct3D 8 calls exist unchanged in Direct3D 9. A Direct3D 11 renderer would be a rewrite.
- Game logic is untouched, so multiplayer sync and replays are unaffected.

## Architecture (SAGE_USE_D3D9)

`Core/Libraries/Source/WWVegas/WW3D2/d3d9compat/` is put in front of the include path, so every
`#include <d3d8.h>` resolves to `d3d8on9.h`:

- Resources (textures, surfaces, vertex/index buffers) are the Direct3D 9 interfaces, named with
  their Direct3D 8 names (macros, so forward declarations still work; see `d3d8fwd.h`).
- `D3D8On9Interface` and `D3D8On9Device` are small adapter classes with the Direct3D 8 methods the
  engine calls, forwarding to Direct3D 9 and translating the differences: sampler states, z-bias,
  stream offsets, base vertex index, DWORD shader handles and Direct3D 8 vertex declarations,
  CopyRects, adapter mode lists, present intervals, caps.
- New code reaches Direct3D 9 directly with `DX8Wrapper::_Get_D3D_Device8()->Get_D3D9()` and calls
  `Invalidate_State()` after changing state the adapter tracks.
- `DX8Wrapper::Init` loads DXVK's `d3d9.dll` / `libdxvk_d3d9.so` instead of the Direct3D 8 library.

The Direct3D 8 build is unchanged; the option is off by default. Windows: `abram_build64.bat d3d9`
(preset `abram-win64-d3d9`).

## Phases

1. **Port** (in progress): the renderer builds and runs on Direct3D 9 with the same picture as
   Direct3D 8. Linux build compiles `z_ww3d2` and the Direct3D-heavy device files; Windows build
   and in-game comparison by the user.
2. **C&C4 procedural damage** on the Crawlers (below): first version in, in-game tuning next.
3. Normal / specular maps, house colour (`RecolorColor`), environment cube; then the rest of
   C&C4's object shader.

## C&C4 procedural damage

What C&C4 does (`Data/Shaders.big`, `cnc4_object_*_proceduraldamage.fxo`, read with MojoShader's
`testparse d3d`):

- The pixel shader samples `DamagedTexture` (`GUObject_Holes`) with the second UV set and does
  `colour *= lerp(sheet, 1, vertex alpha)`; the sheet's alpha (the holes) ends up in the output
  alpha for alpha testing. The vertex shader passes the vertex colour's alpha through unchanged, and
  the models ship it at 1, so the game writes per-vertex damage weights at run time.
- `RenderBuffEffectB`, `FilteredNoise` and `Time` belong to buff effects (shield shimmer), not damage.
- The second UV set places each panel inside one of the sheet's nine blobs (per panel the UVs span
  only a few percent of the sheet).

Ours (`WW3D2/damagelook.*`, `WW3D2/shaders/damagelook.asm`):

- `ps_2_0` with fixed-function vertex processing, so W3D's lighting, fog and skinning are untouched.
- Per-pixel weight from noise instead of per-vertex weights: coarse noise on the damage UVs picks
  when each panel starts burning, fine noise on the base UVs roughens edges; the blob grows from its
  core, embers glow on the growing edge, holes burn through (`texkill`) from 60% damage.
- Amount per object from `RenderObjClass::Set_Damage_Look`, set by `W3DModelDraw`
  (`ProceduralDamage = Yes`), easing toward the real damage.
- Tuning constants are in `damagelook.cpp`; the extractor's `dmg_preview.py` renders the same
  maths offline.

Later: real hit positions (scorch where the shots land), interior faces through holes, normal maps.

## Testing

- Linux build in this environment (`SAGE_USE_D3D9=ON`, compile only: the branch's Windows-only
  features `ModManager` / `GameSwitcher` don't build on Linux).
- Possible later: run the Linux build headless with Mesa's software Vulkan (lavapipe) and the
  player's game files, to compare Direct3D 8 and 9 screenshots.
