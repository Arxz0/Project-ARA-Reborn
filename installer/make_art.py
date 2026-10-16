# Generates the installer's background art and icon (Assets\hero.png, Assets\header.png, Assets\icon.ico).
# Pure procedural art (no game assets): dark gradient, gold globe lines, soft light, film grain.
import math, os, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
A = os.path.join(HERE, 'Assets')
GOLD = (212, 166, 87)

def globe(w, h, cx, cy, r, alpha):
    """Latitude/longitude lines of a tilted globe, in gold, as an RGBA layer."""
    s = 2                                            # supersampling
    L = Image.new('RGBA', (w * s, h * s), (0, 0, 0, 0)); d = ImageDraw.Draw(L)
    tilt = math.radians(18)
    def proj(lat, lon):
        x = math.cos(lat) * math.sin(lon); y = math.sin(lat); z = math.cos(lat) * math.cos(lon)
        y, z = y * math.cos(tilt) - z * math.sin(tilt), y * math.sin(tilt) + z * math.cos(tilt)
        return (cx + r * x) * s, (cy - r * y) * s, z
    def line(pts):
        for (x0, y0, z0), (x1, y1, z1) in zip(pts, pts[1:]):
            front = (z0 + z1) / 2
            a = int(alpha * (0.35 + 0.65 * max(0, front))) if front > -0.15 else int(alpha * 0.12)
            d.line([(x0, y0), (x1, y1)], fill=GOLD + (a,), width=s)
    for lat in range(-75, 90, 15):
        line([proj(math.radians(lat), math.radians(lo)) for lo in range(0, 362, 3)])
    for lon in range(0, 180, 15):
        line([proj(math.radians(la), math.radians(lon)) for la in range(-90, 92, 3)])
        line([proj(math.radians(la), math.radians(lon + 180)) for la in range(-90, 92, 3)])
    d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], outline=GOLD + (int(alpha * 1.4),), width=2 * s)
    return L.resize((w, h), Image.LANCZOS)

def background(w, h, globe_at, seed=7):
    random.seed(seed)
    img = Image.new('RGB', (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            t = x / w; u = y / h
            r = int(12 + 22 * t * (1 - u) + 6 * u); g = int(15 + 14 * t * (1 - u)); b = int(22 + 8 * (1 - t))
            px[x, y] = (r, g, b)
    img = img.convert('RGBA')
    # warm light behind the leaders
    glow = Image.new('RGBA', (w, h), (0, 0, 0, 0)); gd = ImageDraw.Draw(glow)
    gx, gy, gr = globe_at
    for i in range(40, 0, -1):
        a = int(9 + 4.0 * (40 - i) / 40 * 10)
        rr = gr * 1.6 * i / 40
        gd.ellipse([gx - rr, gy - rr, gx + rr, gy + rr], fill=(170, 110, 40, a))
    glow = glow.filter(ImageFilter.GaussianBlur(60))
    img = Image.alpha_composite(img, glow)
    img = Image.alpha_composite(img, globe(w, h, gx, gy, gr, 70))
    # dust / stars
    dust = Image.new('RGBA', (w, h), (0, 0, 0, 0)); dd = ImageDraw.Draw(dust)
    for _ in range(int(w * h / 2500)):
        x, y = random.uniform(0, w), random.uniform(0, h); rr = random.choice([0.6, 0.8, 1.0, 1.4])
        dd.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(240, 215, 160, random.randint(25, 110)))
    img = Image.alpha_composite(img, dust.filter(ImageFilter.GaussianBlur(0.4)))
    # vignette
    vig = Image.new('L', (w, h), 0); vd = ImageDraw.Draw(vig)
    for i in range(60):
        k = i / 60
        vd.rectangle([k * w * 0.35, k * h * 0.35, w - k * w * 0.35, h - k * h * 0.35], fill=int(255 * k))
    vig = vig.filter(ImageFilter.GaussianBlur(80))
    dark = Image.new('RGBA', (w, h), (5, 6, 9, 255))
    img = Image.composite(img, dark, ImageChops.lighter(vig, Image.new('L', (w, h), 70)))
    # grain
    noise = Image.effect_noise((w, h), 18).convert('L')
    img = ImageChops.overlay(img.convert('RGB'), Image.merge('RGB', [noise] * 3).point(lambda v: 128 + (v - 128) // 4))
    return img

def icon():
    S = 512
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle([16, 16, S - 16, S - 16], radius=110, fill=(18, 22, 30, 255), outline=GOLD + (255,), width=14)
    g = globe(S, S, S // 2, S // 2, 170, 120)
    im = Image.alpha_composite(im, g)
    f = ImageFont.truetype(r'C:\Windows\Fonts\bahnschrift.ttf', 300)
    try: f.set_variation_by_name('Bold')
    except Exception: pass
    txt = Image.new('RGBA', (S, S), (0, 0, 0, 0)); td = ImageDraw.Draw(txt)
    td.text((S // 2, S // 2 + 6), 'R', font=f, anchor='mm', fill=(255, 255, 255, 255))
    grad = Image.new('RGBA', (S, S))
    for y in range(S):
        t = y / S
        c = (int(250 - 40 * t), int(222 - 70 * t), int(150 - 80 * t), 255)
        ImageDraw.Draw(grad).line([(0, y), (S, y)], fill=c)
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0)); shadow.putalpha(txt.getchannel('A').filter(ImageFilter.GaussianBlur(10)))
    im = Image.alpha_composite(im, Image.composite(Image.new('RGBA', (S, S), (0, 0, 0, 200)), Image.new('RGBA', (S, S), (0, 0, 0, 0)), shadow.getchannel('A')))
    im = Image.alpha_composite(im, Image.composite(grad, Image.new('RGBA', (S, S), (0, 0, 0, 0)), txt.getchannel('A')))
    im.save(os.path.join(A, 'icon.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    im.resize((128, 128), Image.LANCZOS).save(os.path.join(A, 'logo.png'))

if __name__ == '__main__':
    background(1100, 700, (800, 330, 300)).save(os.path.join(A, 'hero.png'), optimize=True)
    background(1100, 150, (900, 80, 220), seed=3).save(os.path.join(A, 'header.png'), optimize=True)
    icon()
    print('ok')
