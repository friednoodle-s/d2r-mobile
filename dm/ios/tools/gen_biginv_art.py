"""Retile only the bag's 490x196 pixel area; keep all other BC3 blocks exact.

Single-level SpA1 v61 CPU addressing follows dm/gfx/tex/nvn_deswizzle.c.
No compiler needed. Encoding reuses the existing wide-stash BC3 encoder.
"""
import io
from pathlib import Path
import struct
import sys

from PIL import Image
import big_inventory as B
import gen_widestash_art as G
import romfs_art
from gen_paths import gen_dir

BOX = (9, 425, 499, 621)


def blocks(data):
    if data[:6] != b'SpA1\x3d\x00' or struct.unpack_from('<H', data, 20)[0] != 1:
        raise ValueError('big inventory art requires single-frame SpA1 v61')
    w, h = struct.unpack_from('<II', data, 8)
    if (w, h) not in ((512, 700), (507, 700)) or data[22] != 3:
        raise ValueError('unexpected inventory background geometry/format')
    pitch, rows = (w + 3) // 4 * 16, (h + 3) // 4
    th = data[23]
    while th and (8 << (th - 1)) >= rows:
        th -= 1
    bh, gx = 1 << th, (pitch + 63) // 64
    addresses = []
    for y in range(rows):
        for x in range(0, pitch, 16):
            address = ((y // (8 * bh)) * gx * 512 * bh + (x // 64) * 512 * bh
                       + (y % (8 * bh) // 8) * 512 + (x % 64 // 32) * 256
                       + (y % 8 // 2) * 64 + (x % 32 // 16) * 32 + (y % 2) * 16)
            addresses.append(40 + address)
    if max(addresses) + 16 > len(data):
        raise ValueError('truncated inventory sprite')
    return w, h, addresses


def decode(data):
    w, h, addresses = blocks(data)
    linear = b''.join(data[i:i + 16] for i in addresses)
    header = ([124, 0x81007, h, w, len(linear), 0, 1] + [0] * 11
              + [32, 4, struct.unpack('<I', b'DXT5')[0], 0, 0, 0, 0, 0]
              + [0x1000, 0, 0, 0, 0])
    return Image.open(io.BytesIO(b'DDS ' + struct.pack('<31I', *header) + linear)).convert('RGBA')


def retile(image):
    result = image.copy()
    left, top, right, bottom = BOX
    tile = image.crop((left, top, left + 49, top + 49))
    for row in range(B.ROWS):
        y0 = top + round(row * (bottom - top) / B.ROWS)
        y1 = top + round((row + 1) * (bottom - top) / B.ROWS)
        for col in range(B.COLS):
            x0 = left + round(col * (right - left) / B.COLS)
            x1 = left + round((col + 1) * (right - left) / B.COLS)
            result.paste(tile.resize((x1 - x0, y1 - y0), Image.Resampling.LANCZOS), (x0, y0))
    return result


def generate(data):
    w, h, addresses = blocks(data)
    linear = G.bc3(retile(decode(data)))
    result = bytearray(data)
    left, top, right, bottom = BOX
    bw = (w + 3) // 4
    # Preserve every block not intersecting BOX, including equipment, belt, gold.
    for y in range(top // 4, (bottom + 3) // 4):
        for x in range(left // 4, (right + 3) // 4):
            index = y * bw + x
            result[addresses[index]:addresses[index] + 16] = linear[index * 16:index * 16 + 16]
    return bytes(result)


def main(romfs, outdir=None):
    if not B.enabled():
        return
    output = Path(outdir or gen_dir('NativeHUD', 'biginv'))
    output.mkdir(parents=True, exist_ok=True)
    for name in B.SPRITES:
        data = romfs_art.read(romfs, B.SPRITE_DIR + name + '.sprite')
        result = generate(data)
        (output / (name + '.sprite')).write_bytes(result)
        print('big inventory:', name, len(result))


if __name__ == '__main__':
    main(*sys.argv[1:])
