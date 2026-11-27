import sys, os, io, subprocess, tempfile
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen
from PIL import Image

font_path, out = sys.argv[1], sys.argv[2]
INK, DARK = "#f2f2f0", "#0a0b0c"

def glass(uid=""):
    return f'''<defs>
<radialGradient id="bg{uid}" cx="30%" cy="18%" r="95%"><stop offset="0" stop-color="#1d3a31"/><stop offset="0.55" stop-color="#0d1614"/><stop offset="1" stop-color="#060808"/></radialGradient>
<linearGradient id="loop{uid}" x1="60" y1="40" x2="200" y2="220" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#d9ffe9"/><stop offset="0.35" stop-color="#5df0a0"/><stop offset="1" stop-color="#0fb6a2"/></linearGradient>
<linearGradient id="stem{uid}" x1="84" y1="40" x2="84" y2="210" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ffffff" stop-opacity="0.95"/><stop offset="1" stop-color="#ffffff" stop-opacity="0.35"/></linearGradient>
<linearGradient id="shine{uid}" x1="0" y1="90" x2="0" y2="170" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#fff" stop-opacity="0.75"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
<linearGradient id="edge{uid}" x1="0" y1="0" x2="0" y2="256" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#fff" stop-opacity="0.35"/><stop offset="0.4" stop-color="#fff" stop-opacity="0.04"/><stop offset="1" stop-color="#fff" stop-opacity="0.12"/></linearGradient>
<filter id="glow{uid}" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="14"/></filter>
</defs>
<rect width="256" height="256" rx="58" fill="url(#bg{uid})"/>
<rect x="1.5" y="1.5" width="253" height="253" rx="56.5" fill="none" stroke="url(#edge{uid})" stroke-width="3"/>
<g filter="url(#glow{uid})" opacity="0.55" fill="none" stroke="#3ddc84" stroke-width="30" stroke-linecap="round" stroke-linejoin="round"><path d="M84 150A50 50 0 1 1 134 200H116"/><path d="M136 176L112 200L136 224"/></g>
<path d="M84 52V204" stroke="url(#stem{uid})" stroke-width="28" stroke-linecap="round" fill="none"/>
<g fill="none" stroke="url(#loop{uid})" stroke-width="28" stroke-linecap="round" stroke-linejoin="round" opacity="0.94"><path d="M84 150A50 50 0 1 1 134 200H116"/><path d="M136 176L112 200L136 224"/></g>
<path d="M92 142A42 42 0 0 1 168 124" stroke="url(#shine{uid})" stroke-width="6" stroke-linecap="round" fill="none"/>
<path d="M79 60V150" stroke="#fff" stroke-opacity="0.55" stroke-width="4" stroke-linecap="round" fill="none"/>'''

# Small sizes: same mark, slightly heavier, without glow and thin highlights that turn to mud.
SMALL = '''<defs>
<linearGradient id="loop" x1="60" y1="40" x2="200" y2="220" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#b9ffd6"/><stop offset="0.4" stop-color="#4be695"/><stop offset="1" stop-color="#12bba4"/></linearGradient>
</defs>
<rect width="256" height="256" rx="58" fill="#0c1513"/>
<path d="M82 50V206" stroke="#f2f2f0" stroke-width="34" stroke-linecap="round" fill="none"/>
<g fill="none" stroke="url(#loop)" stroke-width="34" stroke-linecap="round" stroke-linejoin="round"><path d="M82 150A52 52 0 1 1 134 202H118"/><path d="M140 176L114 202L140 228"/></g>'''

MONO = '''<g fill="none" stroke="currentColor" stroke-width="28" stroke-linecap="round" stroke-linejoin="round"><path d="M84 52V204"/><path d="M84 150A50 50 0 1 1 134 200H116"/><path d="M136 176L112 200L136 224"/></g>'''

def svg(body, w=256, h=256, title="backhaul"):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" role="img" aria-label="{title}"><title>{title}</title>\n{body}\n</svg>\n'

# --- wordmark as outlines (Unbounded, weight 500) ---
vf = TTFont(font_path)
f = instancer.instantiateVariableFont(vf, {"wght": 500})
gs, cmap, hmtx = f.getGlyphSet(), f.getBestCmap(), f["hmtx"]
upm = f["head"].unitsPerEm
TRACK = -0.01 * upm
def word_path(text, scale, ox, baseline):
    d, x = [], 0.0
    for ch in text:
        g = cmap[ord(ch)]
        pen = SVGPathPen(gs, ntos=lambda v: f"{v:.2f}".rstrip("0").rstrip("."))
        gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, ox + x * scale, baseline)))
        d.append(pen.getCommands())
        x += hmtx[g][0] + TRACK
    return "".join(d), (x - TRACK) * scale
bp = BoundsPen(gs); gs[cmap[ord("b")]].draw(bp)
asc = bp.bounds[3]                      # top of "b" above the baseline, font units
LETTER_H = 104.0
scale = LETTER_H / asc
baseline = 128 + LETTER_H / 2
GAP = 52
d, width = word_path("backhaul", scale, 256 + GAP, baseline)
W = round(256 + GAP + width + 8)

files = {
    "icon.svg": svg(glass()),
    "icon-small.svg": svg(SMALL),
    "mark-mono.svg": svg(MONO),
    "logo-dark.svg": svg(glass() + f'\n<path d="{d}" fill="{INK}"/>', W),
    "logo-light.svg": svg(glass() + f'\n<path d="{d}" fill="{DARK}"/>', W),
}
os.makedirs(out, exist_ok=True)
for name, text in files.items():
    open(os.path.join(out, name), "w", encoding="utf-8").write(text)

def render(svg_file, px_w, px_h):
    with tempfile.TemporaryDirectory() as tmp:
        html = os.path.join(tmp, "p.html")
        png = os.path.join(tmp, "p.png")
        open(html, "w").write(f'<!doctype html><html><body style="margin:0;background:transparent"><img src="file://{os.path.abspath(svg_file)}" style="display:block;width:{px_w}px;height:{px_h}px"></body></html>')
        subprocess.run(["google-chrome", "--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
                        "--default-background-color=00000000", f"--window-size={px_w},{px_h}",
                        f"--screenshot={png}", f"file://{html}"], check=True, capture_output=True)
        im = Image.open(png).convert("RGBA")
        im.load()
        assert im.size == (px_w, px_h), im.size
        return im

big = render(os.path.join(out, "icon.svg"), 1024, 1024)
small = render(os.path.join(out, "icon-small.svg"), 1024, 1024)
big.save(os.path.join(out, "icon-1024.png"))
big.resize((256, 256), Image.LANCZOS).save(os.path.join(out, "icon-256.png"))
frames = [small.resize((s, s), Image.LANCZOS) for s in (16, 20, 24, 32, 40, 48)] + [big.resize((s, s), Image.LANCZOS) for s in (64, 128, 256)]
frames[-1].save(os.path.join(out, "backhaul.ico"), format="ICO", append_images=frames[:-1], sizes=[(i.width, i.height) for i in frames])
print("ok", W, {n: os.path.getsize(os.path.join(out, n)) for n in sorted(os.listdir(out))})
