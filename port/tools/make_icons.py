"""The page's icons (web/favicon-32.png, web/apple-touch-icon.png): a burning diamond with a steel
"6" in it, drawn here (no game art), 8x larger than needed and scaled down.

    python3 -I tools/make_icons.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[1] / 'web'
INK = (12, 8, 6, 255)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))


def icon(size):
    S = size * 8
    u = S / 100  # drawing units
    P = lambda pts: [(x * u, y * u) for x, y in pts]
    # the diamond: fire from gold (top) to deep red (bottom), a dark rim
    grad = Image.new('RGBA', (S, S))
    gd = ImageDraw.Draw(grad)
    stops = [(0.0, (255, 230, 140, 255)), (0.45, (255, 140, 30, 255)), (0.8, (220, 50, 25, 255)), (1.0, (120, 20, 10, 255))]
    for y in range(S):
        t = y / (S - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                gd.line([(0, y), (S, y)], fill=lerp(c0, c1, (t - t0) / (t1 - t0)))
                break
    mask = Image.new('L', (S, S), 0)
    diamond = P([(50, 3), (97, 50), (50, 97), (3, 50)])
    ImageDraw.Draw(mask).polygon(diamond, fill=255)
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    im.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(im)
    d.line(diamond + [diamond[0]], fill=INK, width=int(4 * u), joint='curve')
    # the "6": a steel ring with a hooked stem, outlined in dark, the fire showing through its hole
    steel, rim = (225, 232, 242, 255), INK
    for color, grow in ((rim, 4.5), (steel, 0)):
        g = grow * u
        d.ellipse([33 * u - g, 46 * u - g, 67 * u + g, 80 * u + g], fill=color)
        d.polygon([(33 * u - g, 63 * u), (40 * u - g, 30 * u - g), (62 * u + g, 18 * u - g), (66 * u + g, 27 * u + g),
                   (49 * u, 33 * u + g), (45 * u + g, 63 * u)], fill=color)
    hole = Image.new('L', (S, S), 0)
    ImageDraw.Draw(hole).ellipse([42 * u, 55 * u, 58 * u, 71 * u], fill=255)
    im.paste(grad, (0, 0), hole)
    d.ellipse([42 * u, 55 * u, 58 * u, 71 * u], outline=rim, width=int(3 * u))
    return im.resize((size, size), Image.LANCZOS)


if __name__ == '__main__':
    icon(32).save(OUT / 'favicon-32.png')
    touch = Image.new('RGBA', (180, 180), (12, 14, 18, 255))
    touch.alpha_composite(icon(156), (12, 12))
    touch.convert('RGB').save(OUT / 'apple-touch-icon.png')
    print('icons written to', OUT)
