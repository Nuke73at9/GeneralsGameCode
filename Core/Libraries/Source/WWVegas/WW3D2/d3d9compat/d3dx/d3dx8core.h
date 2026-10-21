// Tiberium Project @feature Direct3D 9 renderer (Windows): the DirectX 8 SDK's D3DX headers include the
// SDK's own "d3d8.h" (a quoted include, found next to them), which clashes with the Direct3D 9 types.
// Use the engine's D3DX headers instead; they are what the d3dx8 library is built from anyway.
#pragma once
#include "../../../../../../../GeneralsMD/Code/CompatLib/Include/d3dx8core.h"
