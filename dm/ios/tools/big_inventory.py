"""Player bag only: fixed HD footprint, 15 x 6 cells. Stack after wide_stash.

The legacy table stores cell dimensions as bytes, so 29 * 2/3 rounds to 19.
Its historical grid endpoints are retained. HD uses its own 98-unit cell size;
do not change the global ItemCellSize (equipment/stash/cube also use it).
"""
import json
from pathlib import Path
import re
import struct

import layout_patch
import romfs_art
import touch_layout
from gen_paths import gen_dir

HERE = Path(__file__).resolve().parent
CONFIG = HERE / 'big_inventory.h'
COLS, ROWS = 15, 6
PLAYER_ROWS = (0, 1, 2, 3, 4, 12, 13, 14, 15, 16, 17, 18, 19, 27, 28, 29)
CLASSES = ('Amazon', 'Sorceress', 'Necromancer', 'Paladin', 'Barbarian',
           'Druid', 'Assassin', 'Warlock')
LAYOUT = 'controller/playerinventoryoriginallayouthd.json'
SPRITES = ('inventorybg', 'inventorybg_classic')
SPRITE_DIR = 'data/hd/global/ui/controller/panel/inventorypanel/v2/'


def enabled():
    m = re.search(r'^#define BIG_INV ([01])$', CONFIG.read_text(encoding='utf-8'), re.M)
    if not m:
        raise ValueError('big_inventory.h must define BIG_INV as 0 or 1')
    return m[1] == '1'


def patch_bin(data):
    if len(data) != 7204 or struct.unpack_from('<I', data)[0] != 30:
        raise ValueError('inventory.bin: expected 30 records of 240 bytes')
    out = bytearray(data)
    for row in PLAYER_ROWS:
        offset = 4 + row * 240
        coords = (339, 626, 255, 368) if row < 15 else (419, 706, 315, 428)
        if (data[offset + 16:offset + 18] != bytes((10, 4))
                or struct.unpack_from('<4i', data, offset + 20) != coords
                or data[offset + 36:offset + 38] != bytes((29, 29))):
            raise ValueError('inventory.bin: unexpected player geometry at row %d' % row)
        out[offset + 16:offset + 18] = bytes((COLS, ROWS))
        out[offset + 36:offset + 38] = bytes((19, 19))
    return bytes(out)


def patch_txt(text):
    lines = text.splitlines(keepends=True)
    header = lines[0].rstrip('\r\n').split('\t')
    indices = {key: header.index(key) for key in
               ('gridX', 'gridY', 'gridBoxWidth', 'gridBoxHeight')}
    found = []
    for i, line in enumerate(lines[1:], 1):
        fields = line.rstrip('\r\n').split('\t')
        if fields[0] not in CLASSES + tuple(c + '2' for c in CLASSES):
            continue
        if [fields[indices[k]] for k in indices] != ['10', '4', '29', '29']:
            raise ValueError('inventory.txt: unexpected player geometry: ' + fields[0])
        found.append(fields[0])
        for key, value in zip(indices, ('15', '6', '19', '19')):
            fields[indices[key]] = value
        ending = '\r\n' if line.endswith('\r\n') else '\n' if line.endswith('\n') else ''
        lines[i] = '\t'.join(fields) + ending
    if sorted(found) != sorted(CLASSES + tuple(c + '2' for c in CLASSES)):
        raise ValueError('inventory.txt: player class set changed')
    return ''.join(lines)


def patch_layout(text):
    obj = layout_patch.load_text(text)
    grids = [c for c in obj['children'] if c.get('name') == 'grid']
    if len(grids) != 1 or grids[0]['type'] != 'InventoryGridWidget':
        raise ValueError('player inventory must have exactly one grid')
    fields = grids[0]['fields']
    if fields['rect'] != {'x': 216, 'y': 893}:
        raise ValueError('player grid origin changed')
    # Inherited from PlayerInventoryOriginalLayoutHD / _profileHD:
    # cellCount 10x4, ItemCellSize {98,98}. Override only this widget.
    fields['cellCount'] = {'x': COLS, 'y': ROWS}
    fields['cellSize'] = {'x': 98 * 10 / COLS, 'y': 98 * 4 / ROWS}
    return json.dumps(obj, ensure_ascii=False, indent=2) + '\n'


def build(romfs, outdir, replacements):
    if not enabled():
        return {}
    result = {}
    for sub in ('', 'base/'):
        for extension in ('bin', 'txt'):
            logical = 'data/global/excel/' + sub + 'inventory.' + extension
            source = replacements.get(logical)
            data = Path(source).read_bytes() if source else romfs_art.read(romfs, logical)
            data = patch_bin(data) if extension == 'bin' else patch_txt(data.decode('utf-8')).encode('utf-8')
            target = Path(outdir, 'bi_' + sub.replace('/', '_') + 'inventory.' + extension)
            target.write_bytes(data)
            result[logical] = str(target)
    logical, original = touch_layout.read_layout(romfs, LAYOUT)
    logical = logical.lower()
    text = Path(replacements[logical]).read_text(encoding='utf-8') if logical in replacements else original
    target = Path(outdir, 'bi_playerinventory.json')
    target.write_text(patch_layout(text), encoding='utf-8', newline='\n')
    result[logical] = str(target)
    for name in SPRITES:
        result[SPRITE_DIR + name + '.sprite'] = str(Path(gen_dir('NativeHUD', 'biginv'), name + '.sprite'))
    return result
