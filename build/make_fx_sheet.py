"""Every effect's editor animation on one sheet: an animated picture for the
README and a video for social media.

Reads the order, names and sections from EFFECTS.md.

    python3 -B build/make_fx_sheet.py           # graphics/fx-all.webp
    python3 -B build/make_fx_sheet.py --video   # video/fx-all.mp4 (not committed)

The picture tiles the 320x160 animations in graphics/fx/<name>.webp
(build/make_fx_previews.py) at their 12 fps; shorter ones loop inside its
3-second cycle. The video records every effect fresh at 450x225 and 30 fps
(build/fx_capture.cjs, needs Playwright, see there) and lays them out on a
3840x2160 (16:9) frame, encoded as H.264 by build/encode_mp4.swift (macOS).
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
BG = (17, 17, 19)          # the previews' own background
INK = (226, 226, 230)
DIM = (128, 128, 138)
FONT = "/System/Library/Fonts/SFNSMono.ttf"
DB_NAME = {"SGNL": "S.GN_L"}                     # EFFECTS.md heading -> effects_db name

# tile = (width, height) of one animation; label = the name strip under it.
PICTURE = dict(cols=6, tile=(320, 160), label=30, gap=8, pad=24, head=64, title=28, sub=15, name=17, sec=12,
               canvas=None, fps=12)
VIDEO = dict(cols=8, tile=(450, 225), label=44, gap=16, pad=64, head=110, title=48, sub=24, name=26, sec=18,
             canvas=(3840, 2160), fps=30, seconds=15, mbit=45)


def font(size):
    try:
        return ImageFont.truetype(FONT, size)
    except OSError:
        return ImageFont.load_default(size)


def effects():
    """(name, file stem, section) in EFFECTS.md order."""
    out, section, stem = [], "", None
    for line in (ROOT / "EFFECTS.md").read_text().splitlines():
        if line.startswith("## "):
            section = line[3:].strip()
        m = re.search(r'src="graphics/fx/([^"]+)\.webp"', line)
        if m:
            stem = m.group(1)
        h = re.match(r"### (.+)", line)
        if h and stem:                 # the heading in the cell next to the picture
            name = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", h.group(1)).strip()   # [Loop](src/...) -> Loop
            out.append((name, stem, section))
            stem = None
    return out


def webp_frames(path):
    im = Image.open(path)
    fr = []
    for i in range(im.n_frames):
        im.seek(i)
        fr.append(im.convert("RGB").copy())
    return fr


def layout(fx, L):
    """The still parts (title, labels) on a blank frame, and where each tile goes."""
    tw, th = L["tile"]
    cols, gap, pad = L["cols"], L["gap"], L["pad"]
    rows = -(-len(fx) // cols)
    gw = cols * tw + (cols - 1) * gap
    gh = L["head"] + rows * (th + L["label"]) + (rows - 1) * gap
    W, H = L["canvas"] or (gw + 2 * pad, gh + 2 * pad)
    x0, y0 = (W - gw) // 2, (H - gh) // 2

    base = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(base)
    d.text((x0, y0), "ZoomMultistompZDL", font=font(L["title"]), fill=INK)
    own = sum(1 for _, _, sec in fx if not sec.startswith("Matujuice"))
    d.text((x0, y0 + round(L["title"] * 1.3)),
           f"{own} custom effects for the Zoom MultiStomp, plus Luca Matturro's {len(fx) - own},"
           " as the MultiStomp Editor draws them", font=font(L["sub"]), fill=DIM)
    slots = []
    fn, fs = font(L["name"]), font(L["sec"])
    for k, (name, stem, section) in enumerate(fx):
        r, c = divmod(k, cols)
        x = x0 + c * (tw + gap)
        y = y0 + L["head"] + r * (th + L["label"] + gap)
        slots.append((stem, x, y))
        ly = y + th + (L["label"] - L["name"]) // 2
        d.text((x + 2, ly), name, font=fn, fill=INK)
        sec = section.split(",")[0].split(" and ")[0]
        d.text((x + tw - 2, ly + L["name"] - L["sec"]), sec, font=fs, fill=DIM, anchor="ra")
    return base, slots


def frame(base, slots, anims, i):
    f = base.copy()
    for stem, x, y in slots:
        a = anims[stem]
        f.paste(a[i % len(a)], (x, y))
    return f


def picture(fx):
    L = PICTURE
    anims = {stem: webp_frames(ROOT / "graphics/fx" / f"{stem}.webp") for _, stem, _ in fx}
    base, slots = layout(fx, L)
    n = max(len(a) for a in anims.values())
    out = [frame(base, slots, anims, i) for i in range(n)]
    dest = ROOT / "graphics" / "fx-all.webp"
    out[0].save(dest, save_all=True, append_images=out[1:], duration=round(1000 / L["fps"]), loop=0,
                quality=80, method=6, minimize_size=True)
    print(f"{dest.relative_to(ROOT)}: {base.width}x{base.height}, {n} frames, {len(fx)} effects, "
          f"{dest.stat().st_size // 1024} KB")


def encoder(td):
    """build/encode_mp4.swift, compiled for this run."""
    exe = Path(td) / "encode_mp4"
    subprocess.run(["swiftc", "-O", str(ROOT / "build" / "encode_mp4.swift"), "-o", str(exe)], check=True)
    return exe


def video(fx):
    L = VIDEO
    n = L["fps"] * L["seconds"]
    with tempfile.TemporaryDirectory() as td:
        names = [DB_NAME.get(name, name) for name, _, _ in fx]
        subprocess.run(["node", str(ROOT / "build" / "fx_capture.cjs"), td, str(n), str(L["fps"]),
                        str(L["tile"][0]), *names], check=True, env=os.environ)
        anims = {}
        for (name, stem, _), db in zip(fx, names):
            anims[stem] = [Image.open(p).convert("RGB") for p in sorted((Path(td) / db).glob("*.png"))]
            if not anims[stem]:
                sys.exit(f"{name}: no frames captured")
        base, slots = layout(fx, L)
        dest = ROOT / "video" / "fx-all.mp4"
        dest.parent.mkdir(exist_ok=True)
        enc = subprocess.Popen([str(encoder(td)), str(dest), str(base.width), str(base.height), str(L["fps"]),
                                str(L["mbit"])], stdin=subprocess.PIPE)
        for i in range(n):
            enc.stdin.write(frame(base, slots, anims, i).tobytes("raw", "BGRX"))
        enc.stdin.close()
        if enc.wait():
            sys.exit("encode failed")
    print(f"{dest.relative_to(ROOT)}: {base.width}x{base.height}, {L['seconds']} s at {L['fps']} fps, "
          f"{dest.stat().st_size // (1 << 20)} MB")


def main():
    fx = effects()
    if "--video" in sys.argv[1:]:
        video(fx)
    else:
        picture(fx)


if __name__ == "__main__":
    sys.exit(main())
