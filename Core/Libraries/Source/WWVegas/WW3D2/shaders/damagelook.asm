; Tiberium Project: C&C4-style procedural damage (ps_2_0, with fixed-function vertex processing).
;
; C&C4's damage sheet (GUObject_Holes) holds nine scorch blobs (RGB, dark on white) with a
; burnt-through hole in each (alpha). The model's second UV set places every panel inside one of
; the blobs. Here noise decides when each panel's blob starts to grow, so scorching appears in
; patches and spreads as health drops, with glowing embers along the growing edge; once the
; object is badly hurt, holes burn through in the blob cores.
;
; t0 base UVs, t1 damage UVs; v0 lit diffuse colour
; s0 base texture, s1 damage sheet, s2 noise (tiling)
; c0 = (spread, noise scale on damage UVs, noise scale on base UVs, sharpness)
;      spread = damage * 1.3: nothing shows at 0, everything is scorched near 1
; c1 = (ember colour rgb, ember strength)
; c2 = (hole amount = gate * maximum, blob growth rate, hole noise scale, scorch strength)

ps_2_0
def c3, -5.0, 1.0, 0.75, 0.5        ; (-ember width, 1, coarse noise weight, 0.5)
def c4, 0.25, -4.0, 4.0, -0.6
dcl t0.xy
dcl t1.xy
dcl v0
dcl_2d s0
dcl_2d s1
dcl_2d s2

texld r0, t0, s0                    ; base colour
texld r1, t1, s1                    ; damage sheet
mul r2.xy, t1, c0.y
texld r2, r2, s2                    ; coarse noise: when each panel starts burning
mul r3.xy, t0, c0.z
add r3.xy, r3, c3.w
texld r3, r3, s2                    ; fine noise: ragged edges
mul r8.xy, t0, c2.z
add r8.xy, r8, c4.x
texld r8, r8, s2                    ; hole noise

lrp r4.x, c3.z, r2.x, r3.x          ; n
add r4.y, c0.x, -r4.x
mul_sat r4.z, r4.y, c0.w            ; q: how far this patch has burnt
add r4.w, r4.z, -r1.x               ; f = q - blob brightness: > 0 inside the grown blob
mul_sat r5.x, r4.w, c2.y
mul r5.x, r5.x, c2.w                ; g: scorch weight

; surface: base x lighting x lerp(1, sheet, g)
lrp r6.xyz, r5.x, r1, c3.y
mul r0.xyz, r0, v0
mul r0.xyz, r0, r6
mul r0.w, r0.w, v0.w

; embers along the growing edge of each blob, cooling as the patch burns out
abs r7.x, r4.w
mad_sat r7.x, r7.x, c3.x, c3.y      ; 1 - |f| * width
mad_sat r7.y, r1.x, c4.y, c4.z      ; inside a blob: (1 - brightness) * 4
mul r7.x, r7.x, r7.y
mad r7.y, r4.z, c4.w, c3.y          ; 1 - 0.6 q
mul r7.x, r7.x, r7.y
mul_sat r7.z, r4.z, c4.z            ; only where the burning has started: q * 4
mul r7.x, r7.x, r7.z
mul r7.x, r7.x, r1.w                ; not inside a hole
mul r7.x, r7.x, c1.w
mad r0.xyz, c1, r7.x, r0

; burnt-through holes: drop the pixel when it is in a blob's hole and the hole noise allows it
add r9.x, r1.w, -c3.w               ; sheet alpha - 0.5
mad r9.y, r4.z, -c2.x, r8.x         ; hole noise - q * hole amount
max r9.x, r9.x, r9.y
mov r9, r9.x
texkill r9

mov oC0, r0
