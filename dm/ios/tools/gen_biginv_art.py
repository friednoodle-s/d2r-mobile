"""Generate either the legacy small bag or the full-cell 13x8 controller art.

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
    if not (0 < w <= 4096 and 0 < h <= 4096) or data[22] != 3:
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
    for row in range(6):
        y0 = top + round(row * (bottom - top) / 6)
        y1 = top + round((row + 1) * (bottom - top) / 6)
        for col in range(15):
            x0 = left + round(col * (right - left) / 15)
            x1 = left + round((col + 1) * (right - left) / 15)
            result.paste(tile.resize((x1 - x0, y1 - y0), Image.Resampling.LANCZOS), (x0, y0))
    return result


def generate_small(data):
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


# Measured controller sprite coordinates: ConsoleRightPanelRect is 1408x1486,
# background is centered, originally (-508,-700), expanded (-674,-860).
# One source pixel = two layout units. The shared outer frame already fits
# the new grid (x=84..1358) and the lower controls (y < 1486).
OLD_ORIGIN = (196, 43)
NEW_ORIGIN = (30, -117)
SIZE = (692, 792)  # Half-resolution MOD footprint, padded to whole BC3 blocks.
SLOTS = {
    'slot_head': (607, 59, 196, 196), 'slot_neck': (851, 221, 98, 98),
    'slot_torso': (607, 281, 196, 294), 'slot_right_arm': (224, 186, 196, 392),
    'slot_left_arm': (994, 188, 196, 392), 'slot_right_hand': (460, 603, 98, 98),
    'slot_left_hand': (857, 603, 98, 98), 'slot_gloves': (224, 603, 196, 196),
    'slot_feet': (994, 601, 196, 196), 'slot_belt': (606, 601, 196, 98),
}


def compose(image):
    from PIL import Image
    result = Image.new('RGBA', SIZE)
    # Copy the ten original slot recesses including their six-pixel borders.
    # Paperdoll silhouettes themselves remain game-drawn backgroundFilename.
    for name, (x, y, w, h) in SLOTS.items():
        left, top = round((x - OLD_ORIGIN[0]) / 2) - 6, round((y - OLD_ORIGIN[1]) / 2) - 6
        box = (left, top, left + w // 2 + 12, top + h // 2 + 12)
        dx, dy = B.SHIFTS[name]
        dest = (left + round((dx + 166) / 2), top + round((dy + 160) / 2))
        result.paste(image.crop(box), dest)
    # Thirteen by eight unscaled 49px tiles = 1274x784 layout units.
    tile = image.crop((9, 425, 58, 474))
    gx, gy = (84 - NEW_ORIGIN[0]) // 2, (549 - NEW_ORIGIN[1]) // 2
    for row in range(8):
        for col in range(13):
            result.paste(tile, (gx + col * 49, gy + row * 49))
    # Original four-cell potion belt, moved by exactly the controller deltas.
    result.paste(image.crop((122, 347, 386, 413)), (212, 725))
    # Compact the gold recess into y=1335..1393, before our Deposit button
    # at y=1394. The amount/button hit regions remain at y=1340.
    result.paste(image.crop((159, 626, 351, 687)).resize((192, 29), Image.Resampling.LANCZOS), (10, 726))
    return result


def encode(image, original):
    """Single-frame BC3 to NVN block-linear, using the existing CPU addresses."""
    w, h = image.size
    hdr = bytearray(original[:40])
    struct.pack_into('<HII', hdr, 6, w - 1, w, h)
    struct.pack_into('<H', hdr, 20, 1)
    pitch, rows = (w + 3) // 4 * 16, (h + 3) // 4
    th = hdr[23]
    while th and (8 << (th - 1)) >= rows:
        th -= 1
    bh = 1 << th
    size = ((pitch + 63) // 64) * ((rows + 8 * bh - 1) // (8 * bh)) * 512 * bh
    struct.pack_into('<I', hdr, 32, size)
    result = bytearray(hdr) + bytearray(size)
    _, _, addresses = blocks(result)
    linear = G.bc3(image)
    for i, address in enumerate(addresses):
        result[address:address + 16] = linear[i * 16:i * 16 + 16]
    return bytes(result)


def generate(data):
    if not B.enabled():
        return data
    w, h, _ = blocks(data)
    if (w, h) not in ((512, 700), (507, 700)):
        raise ValueError('unexpected source inventory background dimensions')
    if B.mode() == 1:
        return generate_small(data)
    return encode(compose(decode(data)), data)


def main(romfs, outdir=None):
    if not B.enabled():
        return
    output = Path(outdir or gen_dir('NativeHUD', 'biginv'))
    output.mkdir(parents=True, exist_ok=True)
    for name in B.SPRITES:
        data = romfs_art.read(romfs, B.SPRITE_DIR + name + '.sprite')
        result = generate(data)
        (output / (name + '.sprite')).write_bytes(result)
        # Explicit inspection output only; build caches contain sprites alone.
        if outdir:
            decode(result).save(output / (name + '.png'))
        print('big inventory:', name, len(result))


if __name__ == '__main__':
    main(*sys.argv[1:])
