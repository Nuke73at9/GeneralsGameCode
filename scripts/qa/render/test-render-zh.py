#!/usr/bin/env python3
"""Renderer check for Zero Hour (Tiberium Project): fixed maps and camera views, compared with
reference screenshots, plus a logic check that the renderer never changes the game.

Each scene starts the game with the engine's test options (-testMap, -testCamera, -testShots,
-testQuit; see Core/GameEngine/Include/Common/TiberiumTest.h), collects the screenshots it takes
and compares them with the reference set. The logic check runs a campaign map twice, with the
C&C4 renderer on and off (CNC4Style in Options.ini), and compares the logic CRC each run logs.

Reference screenshots show the game's own art, so they are kept out of the repository: --refs
points to a folder on your machine. The first run with --update stores them.

  python test-render-zh.py --game <exe> --userdata <folder> --refs <folder> [--out <folder>]
                           [--prefix "<launcher words>"] [--args "<extra game arguments>"]
                           [--scenes alpine_town,alpine_mountain] [--options "Key = value; ..."]
                           [--update] [--no-logic]

  --game      the game executable (GeneralsXZH.exe), or a launcher script when --prefix is used
  --userdata  the game's user data folder (Documents\\Command and Conquer Generals Zero Hour Data
              on Windows): where Options.ini, Screenshots and TiberiumTest.log live
  --prefix    words put before the game, e.g. a wrapper that sets up a virtual display
  --args      extra game arguments for every run, e.g. "-win -mod <mod folder>"
  --options   Options.ini lines for the scene runs, separated by ";", e.g. "Sage2ObjectShaders = forward"
              (Options.ini is put back afterwards)
  --threshold mean colour difference (0-255) above which a screenshot counts as changed (2.0)

Exit status: 0 when every compared screenshot is within the threshold and the logic CRCs match.
Needs Python 3.8+; Pillow for the image comparison (without it only the logic check runs).
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

# name: (map, camera steps, screenshot every N frames, quit frame, Options.ini lines)
SCENES = {
    "alpine_town": (r"Maps\Alpine Assault\Alpine Assault.map", "10,0.45,-,1500,950", 100, 200, []),
    "alpine_mountain": (r"Maps\Alpine Assault\Alpine Assault.map", "10,0.3,-,239,217", 100, 200, []),
    "alpine_start": (r"Maps\Alpine Assault\Alpine Assault.map", "", 100, 200, []),
    "beach_scorch": (r"Maps\Bombardment Beach\Bombardment Beach.map", "10,0.6", 100, 200, []),
}
SCENE_EXTRA_ARGS = {"beach_scorch": ["-testScorch", "40"]}
LOGIC_MAP = r"Maps\MD_GLA01\MD_GLA01.map"
LOGIC_FRAME = 300


def run_game(opts, extra, options_lines, timeout):
    """Starts the game with the test options; returns the new screenshots and TiberiumTest.log."""
    userdata = opts.userdata
    shots_dir = os.path.join(userdata, "Screenshots")
    os.makedirs(shots_dir, exist_ok=True)
    before = set(os.listdir(shots_dir)) | set(os.listdir(userdata))
    options = os.path.join(userdata, "Options.ini")
    saved = None
    if options_lines:
        with open(options, "r", encoding="latin1") as f:
            saved = f.read()
        with open(options, "w", encoding="latin1") as f:
            f.write(saved.rstrip("\n") + "\n" + "\n".join(options_lines) + "\n")
    command = shlex.split(opts.prefix) + [opts.game] + shlex.split(opts.args) + ["-nologo", "-noshellmap"] + extra
    try:
        subprocess.run(command, timeout=timeout, cwd=os.path.dirname(os.path.abspath(opts.game)) or None,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        print("  the game did not quit in %d s" % timeout)
    finally:
        if saved is not None:
            with open(options, "w", encoding="latin1") as f:
                f.write(saved)
    time.sleep(1)
    new = [os.path.join(shots_dir, n) for n in sorted(os.listdir(shots_dir)) if n not in before and n.lower().endswith(".png")]
    # the Linux build writes them next to Options.ini, named "Screenshots\\sshot_...png"
    new += [os.path.join(userdata, n) for n in sorted(os.listdir(userdata))
            if n not in before and n.lower().startswith("screenshots\\") and n.lower().endswith(".png")]
    log = ""
    log_path = os.path.join(userdata, "TiberiumTest.log")
    if os.path.exists(log_path):
        with open(log_path, "r", encoding="latin1") as f:
            log = f.read()
    return new, log


def compare(a_path, b_path):
    """Mean absolute colour difference (0-255) and the share of pixels off by more than 16."""
    from PIL import Image, ImageChops
    a = Image.open(a_path).convert("RGB")
    b = Image.open(b_path).convert("RGB")
    if a.size != b.size:
        return 255.0, 1.0
    diff = ImageChops.difference(a, b).convert("L")
    hist = diff.histogram()
    total = a.size[0] * a.size[1]
    mean = sum(i * n for i, n in enumerate(hist)) / total
    off = sum(hist[17:]) / total
    return mean, off


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--game", required=True)
    p.add_argument("--userdata", required=True)
    p.add_argument("--refs", required=True)
    p.add_argument("--out", default="render-check")
    p.add_argument("--prefix", default="")
    p.add_argument("--args", default="")
    p.add_argument("--scenes", default=",".join(SCENES))
    p.add_argument("--options", default="")
    p.add_argument("--threshold", type=float, default=2.0)
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--update", action="store_true", help="store this run's screenshots as the references")
    p.add_argument("--no-logic", action="store_true", help="skip the logic check")
    opts = p.parse_args()

    try:
        import PIL  # noqa: F401
        have_pil = True
    except ImportError:
        have_pil = False
        print("Pillow is not installed: screenshots are collected but not compared")

    failures = 0
    for name in [s for s in opts.scenes.split(",") if s]:
        if name not in SCENES:
            print("unknown scene %s" % name)
            failures += 1
            continue
        game_map, camera, every, quit_frame, options_lines = SCENES[name]
        extra = ["-testMap", game_map, "-testShots", str(every), "-testQuit", str(quit_frame)]
        if camera:
            extra += ["-testCamera", camera]
        extra += SCENE_EXTRA_ARGS.get(name, [])
        print("scene %s" % name)
        options_lines = options_lines + [line.strip() for line in opts.options.split(";") if line.strip()]
        shots, _ = run_game(opts, extra, options_lines, opts.timeout)
        out_dir = os.path.join(opts.out, name)
        os.makedirs(out_dir, exist_ok=True)
        kept = []
        for i, shot in enumerate(shots):
            target = os.path.join(out_dir, "%02d.png" % (i + 1))
            shutil.move(shot, target)
            kept.append(target)
        ref_dir = os.path.join(opts.refs, name)
        if opts.update:
            os.makedirs(ref_dir, exist_ok=True)
            for k in kept:
                shutil.copy(k, os.path.join(ref_dir, os.path.basename(k)))
            print("  %d screenshots stored as references" % len(kept))
            continue
        if not kept:
            print("  FAIL: no screenshots")
            failures += 1
            continue
        if not have_pil:
            continue
        for k in kept:
            ref = os.path.join(ref_dir, os.path.basename(k))
            if not os.path.exists(ref):
                print("  %s: no reference" % os.path.basename(k))
                continue
            mean, off = compare(k, ref)
            verdict = "ok" if mean <= opts.threshold else "CHANGED"
            if verdict != "ok":
                failures += 1
            print("  %s: mean difference %.2f, %.1f%% of pixels off by more than 16: %s" % (os.path.basename(k), mean, off * 100, verdict))

    if not opts.no_logic:
        crcs = {}
        for mode in ("yes", "no"):
            _, log = run_game(opts, ["-testMap", LOGIC_MAP, "-testCRC", str(LOGIC_FRAME), "-testQuit", str(LOGIC_FRAME + 20)],
                              ["CNC4Style = %s" % mode], opts.timeout)
            m = re.search(r"logic frame (\d+): CRC ([0-9A-F]{8})", log)
            crcs[mode] = m.group(2) if m else None
            print("logic check, CNC4Style = %s: %s" % (mode, crcs[mode] or "no CRC logged"))
        if crcs["yes"] is None or crcs["yes"] != crcs["no"]:
            print("logic check FAILED: the renderer changed the game")
            failures += 1
        else:
            print("logic check ok: the same game with the C&C4 renderer on and off")

    print("%s" % ("all checks passed" if failures == 0 else "%d check(s) failed" % failures))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
