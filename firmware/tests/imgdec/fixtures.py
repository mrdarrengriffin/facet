# Fixtures for the panel picture decode test (run.sh): made with Pillow into the folder given (never committed).
import os, random, sys
from PIL import Image
d = sys.argv[1]
random.seed(7)
def photo(w, h):
    im = Image.new('RGB', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 255) // w, (y * 255) // h, ((x ^ y) * 3) & 255)
    return im
p = photo(640, 480)
p.save(f'{d}/photo.jpg', quality=90)                                  # baseline
p.save(f'{d}/progressive.jpg', quality=90, progressive=True)
p.convert('L').save(f'{d}/grey.jpg', quality=90)
p.save(f'{d}/photo.png')
p.convert('RGBA').save(f'{d}/rgba.png')
p.convert('P', palette=Image.ADAPTIVE).save(f'{d}/palette.png')
Image.new('I;16', (64, 48), 30000).save(f'{d}/grey16.png')
Image.new('RGB', (8000, 6000), (40, 90, 160)).save(f'{d}/huge-flat.jpg', quality=80)   # few bytes, 48 Mpx
Image.new('RGB', (5000, 5000), (200, 30, 30)).save(f'{d}/huge-flat.png')              # few bytes, 25 Mpx
noise = Image.frombytes('RGB', (2400, 1800), bytes(random.getrandbits(8) for _ in range(2400 * 1800 * 3)))
noise.save(f'{d}/big.jpg', quality=95)                                # several MB: over every body cap
p.save(f'{d}/photo.gif'); p.save(f'{d}/photo.webp')
open(f'{d}/page.html', 'w').write('<!doctype html><html><body>Just a moment...</body></html>')
