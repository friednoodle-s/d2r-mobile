"""Player-only bag modes, composed after the existing touch and stash patches.

Mode 1 retains the old fixed-footprint 15x6 experiment. Mode 2 uses the
ExpandedInventory controller arrangement with the original 98-unit cells.
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
COLS, ROWS = 13, 8
PLAYER_ROWS = (0, 1, 2, 3, 4, 12, 13, 14, 15, 16, 17, 18, 19, 27, 28, 29)
CLASSES = ('Amazon', 'Sorceress', 'Necromancer', 'Paladin', 'Barbarian',
           'Druid', 'Assassin', 'Warlock')
LAYOUT = 'controller/playerinventoryoriginallayouthd.json'
EXPANSION = 'controller/playerinventoryexpansionlayouthd.json'
SWITCH = 'controller/console/switch/playerinventoryexpansionlayouthd.json'
SPRITES = ('inventorybg', 'inventorybg_classic')
SPRITE_DIR = 'data/hd/global/ui/controller/panel/inventorypanel/v2/'
# olegbl ExpandedInventory/mod.js, controller branch at d999b4b.
# Gold is 14 units above the MOD position to fit our Deposit button below it.
SHIFTS = {
    'slot_head': (-239, 21), 'slot_neck': (-3, -167),
    'slot_torso': (6, -199), 'slot_right_arm': (-99, -60),
    'slot_left_arm': (123, -62), 'slot_right_hand': (389, -417),
    'slot_left_hand': (126, -417), 'slot_gloves': (146, -282),
    'slot_feet': (-130, -281), 'slot_belt': (7, -185),
    'Belt': (15, 595), 'gold_amount': (-464, 6),
    'gold_button': (-464, 6), 'background': (-166, -160),
}


def mode():
    m = re.search(r'^#define BIG_INV ([012])$', CONFIG.read_text(encoding='utf-8'), re.M)
    if not m:
        raise ValueError('big_inventory.h must define BIG_INV as 0, 1 or 2')
    return int(m[1])


def enabled():
    return mode() != 0


def layouts():
    return (LAYOUT, EXPANSION, SWITCH) if mode() == 2 else (LAYOUT,)


def geometry(row):
    return (339, 626, 255, 368) if row < 15 else (419, 706, 315, 428)


def expanded_geometry(coords):
    # Map the HD origin delta into the legacy 29-unit cells, preserving its
    # original coordinate-system offset. Endpoints now describe full cells.
    left = coords[0] + round((84 - 216) * 29 / 98)
    top = coords[2] + round((549 - 893) * 29 / 98)
    return left, left + COLS * 29, top, top + ROWS * 29


def patch_bin(data):
    current = mode()
    if not current:
        return data
    if len(data) != 7204 or struct.unpack_from('<I', data)[0] != 30:
        raise ValueError('inventory.bin: expected 30 records of 240 bytes')
    out = bytearray(data)
    for row in PLAYER_ROWS:
        offset = 4 + row * 240
        coords = geometry(row)
        if (data[offset + 16:offset + 18] != bytes((10, 4))
                or struct.unpack_from('<4i', data, offset + 20) != coords
                or data[offset + 36:offset + 38] != bytes((29, 29))):
            raise ValueError('inventory.bin: unexpected player geometry at row %d' % row)
        out[offset + 16:offset + 18] = bytes((15, 6) if current == 1 else (COLS, ROWS))
        if current == 1:
            out[offset + 36:offset + 38] = bytes((19, 19))
        else:
            struct.pack_into('<4i', out, offset + 20, *expanded_geometry(coords))
    return bytes(out)


def patch_txt(text):
    current = mode()
    if not current:
        return text
    lines = text.splitlines(keepends=True)
    header = lines[0].rstrip('\r\n').split('\t')
    keys = ('gridX', 'gridY', 'gridBoxWidth', 'gridBoxHeight',
            'gridLeft', 'gridRight', 'gridTop', 'gridBottom')
    indices = {key: header.index(key) for key in keys}
    found = []
    names = CLASSES + tuple(c + '2' for c in CLASSES)
    for i, line in enumerate(lines[1:], 1):
        fields = line.rstrip('\r\n').split('\t')
        if fields[0] not in names:
            continue
        coords = geometry(15 if fields[0].endswith('2') else 0)
        if [int(fields[indices[k]]) for k in keys] != [10, 4, 29, 29, *coords]:
            raise ValueError('inventory.txt: unexpected player geometry: ' + fields[0])
        found.append(fields[0])
        values = (15, 6, 19, 19, *coords) if current == 1 else (COLS, ROWS, 29, 29, *expanded_geometry(coords))
        for key, value in zip(keys, values):
            fields[indices[key]] = str(value)
        ending = '\r\n' if line.endswith('\r\n') else '\n' if line.endswith('\n') else ''
        lines[i] = '\t'.join(fields) + ending
    if sorted(found) != sorted(names):
        raise ValueError('inventory.txt: player class set changed')
    return ''.join(lines)


def _shift(children, name, dx, dy):
    rect = children[name]['fields']['rect']
    # Touch layout hides controller legends at x=99999. Keep them hidden.
    if abs(rect['x']) < 90000 and abs(rect['y']) < 90000:
        rect['x'] += dx
        rect['y'] += dy


def pouch_positions():
    # Shared with the host's per-frame custom-option visibility code.
    text = CONFIG.read_text(encoding='utf-8')
    def value(key):
        return int(re.search(r'^#define BIG_INV2_' + key + r' (\d+)$', text, re.M)[1])
    x, y = value('POUCH_X'), value('POUCH_Y')
    return {'TouchPouch': (x, y), 'TouchPouchIcon': (x + 24, y + 16),
            'TouchPouchCount': (x + 128, y),
            'TouchStore': (value('STORE_X'), value('STORE_Y'))}


def patch_layout(text, rel=LAYOUT):
    current = mode()
    if not current:
        return text
    obj = layout_patch.load_text(text)
    children = {c['name']: c for c in obj['children']}
    if rel == LAYOUT:
        grid = children['grid']
        fields = grid['fields']
        if grid['type'] != 'InventoryGridWidget' or fields['rect'] != {'x': 216, 'y': 893}:
            raise ValueError('player grid origin/type changed')
        fields['cellCount'] = {'x': 15, 'y': 6} if current == 1 else {'x': COLS, 'y': ROWS}
        fields['cellSize'] = {'x': 98 * 2 / 3, 'y': 98 * 2 / 3} if current == 1 else {'x': 98, 'y': 98}
        if current == 2:
            fields['rect'] = {'x': 84, 'y': 549}
            for name, (dx, dy) in SHIFTS.items():
                _shift(children, name, dx, dy)
    elif current == 2 and rel == EXPANSION:
        for side, dx, dy in (('right', -99, -60), ('left', 123, -62)):
            names = ('background_' + side + '_arm', 'background_' + side + '_arm_selected',
                     'WeaponSwap' + side.title() + 'Legend')
            names += ('text_i_' + ('left' if side == 'right' else 'right'),
                      'text_ii_' + ('left' if side == 'right' else 'right'))
            for name in names:
                _shift(children, name, dx, dy)
    elif current == 2 and rel == SWITCH:
        _shift(children, 'weaponswap_right_arm', -99, -60)
        _shift(children, 'weaponswap_left_arm', 123, -62)
        pouch = children.get('TouchPouchBox')
        if pouch:
            positions = pouch_positions()
            for child in pouch['children']:
                if child['name'] in positions:
                    x, y = positions[child['name']]
                    child['fields']['rect'].update(x=x, y=y)
                    if child['name'] == 'TouchStore':
                        child['fields']['rect']['scale'] = 1.0
    else:
        raise ValueError('unexpected inventory layout: ' + rel)
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
    for rel in layouts():
        logical, original = touch_layout.read_layout(romfs, rel)
        logical = logical.lower()
        text = Path(replacements[logical]).read_text(encoding='utf-8') if logical in replacements else original
        target = Path(outdir, 'bi_' + rel.replace('/', '_'))
        target.write_text(patch_layout(text, rel), encoding='utf-8', newline='\n')
        result[logical] = str(target)
    for name in SPRITES:
        result[SPRITE_DIR + name + '.sprite'] = str(Path(gen_dir('NativeHUD', 'biginv'), name + '.sprite'))
    return result
