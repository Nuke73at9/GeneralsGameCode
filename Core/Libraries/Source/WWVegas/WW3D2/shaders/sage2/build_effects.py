"""Compile the Tiberium Project's SAGE 2 effects (Tiberium Project).
  python build_effects.py <C&C4 shader source dir> [path to fxc.exe]
The effects include C&C4's own shader headers (the GPL modding-support sources: Common.fxh,
Gamma.fxh, DeferredLighting.fxh ...), so they take the same engine values as C&C4's effects.
C&C4 built its effects with _WW3D_ defined (indirect constants, as the shipped .fxo files show).
The sources go through the C preprocessor first: today's fxc rejects C&C4's REGISTER_BIND
macros (arguments that expand to nothing), the preprocessor does not. On Linux/macOS fxc runs
under Wine. The compiled *.fxo files are committed; packed into the mod as Art/Shaders/<name>.fxo.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
EFFECTS = ["tiberium_terrain.fx", "tiberium_topview.fx", "tiberium_road.fx", "tiberium_tracks.fx"]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    include = sys.argv[1]
    fxc = sys.argv[2] if len(sys.argv) > 2 else os.environ.get("FXC", "fxc.exe")
    fxc_cmd = [fxc] if os.name == "nt" else ["wine", fxc]
    env = dict(os.environ, WINEDEBUG="-all")
    # fxc's own preprocessor expands a macro argument before pasting it (register(c##index));
    # the C preprocessor does not, so RegisterMap.fxh gets a two-level paste in a shadow copy
    shim = tempfile.mkdtemp(prefix="sage2fx_")
    for name in os.listdir(include):	# a quoted include looks next to the including file first, so the whole set is shadowed
        if name.lower().endswith(".fxh"):
            shutil.copy(os.path.join(include, name), shim)
    with open(os.path.join(include, "RegisterMap.fxh"), encoding="latin1") as f:
        reg = f.read()
    reg = reg.replace("#define REGISTER_EVAL(x) register(c##x)",
                      "#define REGISTER_PASTE(a, b) a##b\n#define REGISTER_EVAL(x) register(REGISTER_PASTE(c, x))")
    reg = reg.replace("#define REGISTER_EVAL_PS(x) register(ps, c##x)", "#define REGISTER_EVAL_PS(x) register(ps, REGISTER_PASTE(c, x))")
    reg = reg.replace("#define REGISTER_EVAL_VS(x) register(vs, c##x)", "#define REGISTER_EVAL_VS(x) register(vs, REGISTER_PASTE(c, x))")
    reg = re.sub(r"register\((vs, |ps, )?([cib])##registerIndex\)", lambda m: f"register({m.group(1) or ''}REGISTER_PASTE({m.group(2)}, registerIndex))", reg)
    with open(os.path.join(shim, "RegisterMap.fxh"), "w", encoding="latin1") as f:
        f.write(reg)
    for fx in EFFECTS:
        src = os.path.join(HERE, fx)
        pre = os.path.join(HERE, "_pre_" + fx)
        out = fx[:-3] + ".fxo"
        r = subprocess.run(["cpp", "-P", "-nostdinc", "-D", "EA_PLATFORM_WINDOWS", "-D", "_WW3D_", "-I", shim, "-I", HERE, src, "-o", pre],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"{fx}: preprocessing failed\n{r.stderr}")
        r = subprocess.run(fxc_cmd + ["/nologo", "/T", "fx_2_0", "/Fo", out, os.path.basename(pre)], cwd=HERE, env=env,
                           capture_output=True, text=True)
        os.remove(pre)
        errors = [l for l in (r.stdout + r.stderr).splitlines() if "error" in l.lower() and "deprecated" not in l]
        if r.returncode != 0 or errors:
            sys.exit(f"{fx}: fxc failed\n" + "\n".join(errors or [r.stdout, r.stderr]))
        print(f"{fx} -> {out} ({os.path.getsize(os.path.join(HERE, out))} bytes)")
    shutil.rmtree(shim, ignore_errors=True)


if __name__ == "__main__":
    main()
