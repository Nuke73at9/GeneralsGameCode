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
2. **C&C4 procedural damage prototype** on the Crawlers (below).
3. Normal / specular maps, house colour (`RecolorColor`), environment cube; then the rest of
   C&C4's object shader.

## C&C4 procedural damage (findings)

From `Data/Shaders.big`, `shaders/compiled/cnc4_object_gdi_proceduraldamage.fxo` (RefPack
compressed `fx_2_0` effect; disassembles with MojoShader's `testparse d3d`):

- Techniques `Default` / `Default_M` / `Default_L` (quality levels) plus shadow and depth passes;
  shaders are `vs_3_0` / `ps_3_0` (and `ps_2_0` for low quality).
- Damage inputs: `DamagedTexture` (the unit's damage decal sheet, e.g. `GUObject_Holes`),
  `FilteredNoise` (WW3D.FilteredNoise), and `RenderBuffEffectB` (WW3D.RenderBuffEffectB, a
  per-object value; very likely the damage amount). Damage is therefore procedural from the
  object's health (noise thresholded by the damage amount reveals scorch and holes), not per hit.
- Next: read the high-quality pixel shader to confirm how `RenderBuffEffectB` and the noise are
  combined, then write the equivalent `ps_3_0` (or use C&C4's bytecode, loaded from the player's
  own install like the art) with the object's damage fed in as a constant.

## Testing

- Linux build in this environment (`SAGE_USE_D3D9=ON`, compile only: the branch's Windows-only
  features `ModManager` / `GameSwitcher` don't build on Linux).
- Possible later: run the Linux build headless with Mesa's software Vulkan (lavapipe) and the
  player's game files, to compare Direct3D 8 and 9 screenshots.
