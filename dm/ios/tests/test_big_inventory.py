"""Player-only data isolation, controller inheritance and sprite regressions."""
import json
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import big_inventory as B
import gen_biginv_art as A
from PIL import Image


def table():
    data = bytearray((i * 17 % 256 for i in range(7204)))
    struct.pack_into('<I', data, 0, 30)
    for row in B.PLAYER_ROWS:
        offset = 4 + row * 240
        data[offset + 16:offset + 18] = bytes((10, 4))
        struct.pack_into('<4i', data, offset + 20,
                         *((339, 626, 255, 368) if row < 15 else (419, 706, 315, 428)))
        data[offset + 36:offset + 38] = bytes((29, 29))
    return bytes(data)


def text_table():
    header = 'class\tgridX\tgridY\tgridBoxWidth\tgridBoxHeight\tgridLeft\tgridRight\tgridTop\tgridBottom\r\n'
    names = B.CLASSES + tuple(c + '2' for c in B.CLASSES)
    rows = []
    for name in names:
        coords = (419, 706, 315, 428) if name.endswith('2') else (339, 626, 255, 368)
        rows.append(name + '\t10\t4\t29\t29\t' + '\t'.join(map(str, coords)) + '\r\n')
    return header + ''.join(rows) + 'Trade Page 1\t10\t4\t29\t29\t20\t100\t20\t100\r\n'


def original_layout():
    children = [{'type': 'InventoryGridWidget', 'name': 'grid', 'fields': {
        'rect': {'x': 216, 'y': 893}, 'navigation': {'left': {'panelBoundary': True}}}}]
    for name, rect in A.SLOTS.items():
        children.append({'name': name, 'fields': {'rect': dict(zip(('x', 'y', 'width', 'height'), rect))}})
    for name, rect in {'Belt': (452, 750), 'gold_amount': (605, 1334),
                       'gold_button': (551, 1334), 'background': (-508, -700)}.items():
        children.append({'name': name, 'fields': {'rect': dict(zip(('x', 'y'), rect))}})
    children.append({'name': 'Unrelated', 'fields': {'rect': {'x': 123, 'y': 456}}})
    return {'fields': {'rect': '$ConsoleRightPanelRect'}, 'children': children}


def sprite():
    data = bytearray(524328)
    data[:6] = b'SpA1\x3d\x00'
    struct.pack_into('<HII', data, 6, 507, 512, 700)
    struct.pack_into('<H', data, 20, 1)
    data[22:24] = bytes((3, 4))
    struct.pack_into('<I', data, 32, len(data) - 40)
    return bytes(data)


class BigInventoryTests(unittest.TestCase):
    def test_mode_parser_and_default(self):
        self.assertEqual(B.mode(), 2)
        for current in (0, 1, 2):
            with patch.object(B.CONFIG.__class__, 'read_text', return_value='#define BIG_INV %d\n' % current):
                self.assertEqual(B.mode(), current)
                self.assertEqual(B.enabled(), bool(current))
        with patch.object(B.CONFIG.__class__, 'read_text', return_value='#define BIG_INV 3\n'):
            with self.assertRaises(ValueError):
                B.mode()

    def test_bin_full_cells_endpoints_and_nonplayer_isolation(self):
        original = table()
        with patch.object(B, 'mode', return_value=2):
            result = B.patch_bin(original)
        self.assertEqual(len(result), len(original))
        for row in range(30):
            start = 4 + row * 240
            old, new = original[start:start + 240], result[start:start + 240]
            if row not in B.PLAYER_ROWS:
                self.assertEqual(old, new)
                continue
            self.assertEqual(new[16:18], bytes((13, 8)))
            self.assertEqual(new[36:38], bytes((29, 29)))
            self.assertEqual(new[:16], old[:16])
            self.assertEqual(new[18:20], old[18:20])
            self.assertEqual(new[36:], old[36:])
            self.assertEqual(struct.unpack_from('<4i', new, 20),
                             (300, 677, 153, 385) if row < 15 else (380, 757, 213, 445))

    def test_small_mode_preserves_legacy_behavior(self):
        original = table()
        with patch.object(B, 'mode', return_value=1):
            result = B.patch_bin(original)
            out = json.loads(B.patch_layout(json.dumps(original_layout())))
            text = B.patch_txt(text_table())
        expected = {4 + row * 240 + offset for row in B.PLAYER_ROWS for offset in (16, 17, 36, 37)}
        self.assertEqual({i for i, (a, b) in enumerate(zip(original, result)) if a != b}, expected)
        grid = out['children'][0]['fields']
        self.assertEqual(grid['rect'], {'x': 216, 'y': 893})
        self.assertAlmostEqual(grid['cellSize']['x'] * grid['cellCount']['x'], 980)
        self.assertAlmostEqual(grid['cellSize']['y'] * grid['cellCount']['y'], 392)
        self.assertEqual(text.count('\t15\t6\t19\t19\t'), 16)

    def test_fail_closed_on_input_drift(self):
        for data in (B.patch_bin(table()), table()[:-1]):
            with self.assertRaises(ValueError):
                B.patch_bin(data)
        with self.assertRaises(ValueError):
            B.patch_txt(text_table().replace('Amazon\t10', 'Amazon\t9'))

    def test_text_matches_bin_and_preserves_other_rows_and_crlf(self):
        original = text_table()
        result = B.patch_txt(original)
        self.assertEqual(result.splitlines(keepends=True)[-1], original.splitlines(keepends=True)[-1])
        self.assertEqual(result.count('\t13\t8\t29\t29\t'), 16)
        self.assertIn('Amazon\t13\t8\t29\t29\t300\t677\t153\t385\r\n', result)
        self.assertIn('Amazon2\t13\t8\t29\t29\t380\t757\t213\t445\r\n', result)

    def test_layout_full_cell_geometry_and_slot_separation(self):
        obj = original_layout()
        result = json.loads(B.patch_layout(json.dumps(obj)))
        fields = {c['name']: c['fields'] for c in result['children']}
        grid = fields['grid']
        self.assertEqual(grid['rect'], {'x': 84, 'y': 549})
        self.assertEqual(grid['cellCount'], {'x': 13, 'y': 8})
        self.assertEqual(grid['cellSize'], {'x': 98, 'y': 98})
        self.assertEqual(grid['navigation'], obj['children'][0]['fields']['navigation'])
        self.assertEqual(result['children'][-1], obj['children'][-1])
        self.assertEqual(result['fields'], obj['fields'])
        for name in A.SLOTS:
            r = fields[name]['rect']
            self.assertLessEqual(r['y'] + r['height'], 549)
            self.assertGreaterEqual(r['x'], 0)
            self.assertLessEqual(r['x'] + r['width'], 1408)
        self.assertEqual(fields['Belt']['rect'], {'x': 467, 'y': 1345})
        self.assertEqual(fields['gold_button']['rect'], {'x': 87, 'y': 1340})

    def test_switch_hit_boxes_and_custom_controls(self):
        obj = {'children': [
            {'name': 'weaponswap_right_arm', 'fields': {'rect': {'x': 215, 'y': 134, 'width': 216, 'height': 59}}},
            {'name': 'weaponswap_left_arm', 'fields': {'rect': {'x': 985, 'y': 134, 'width': 216, 'height': 59}}},
            {'name': 'TouchClose', 'fields': {'rect': {'x': 2079, 'y': -130}}},
            {'name': 'TouchPouchBox', 'children': [
                {'name': 'TouchPouch', 'fields': {'rect': {'x': 222, 'y': 1294, 'width': 280, 'height': 128}}},
                {'name': 'TouchPouchTip', 'fields': {'rect': {'x': 99999, 'y': 0}}},
                {'name': 'TouchStore', 'fields': {'rect': {'x': 916, 'y': 1323, 'width': 366, 'scale': 0.74}}}]}]}
        result = json.loads(B.patch_layout(json.dumps(obj), B.SWITCH))
        self.assertEqual(result['children'][0]['fields']['rect'], {'x': 116, 'y': 74, 'width': 216, 'height': 59})
        self.assertEqual(result['children'][1]['fields']['rect']['x'], 1108)
        self.assertEqual(result['children'][2], obj['children'][2])
        pouch, tip, store = result['children'][3]['children']
        self.assertEqual(pouch['fields']['rect'], {'x': 1100, 'y': 1340, 'width': 280, 'height': 128})
        self.assertEqual(tip, obj['children'][3]['children'][1])
        self.assertEqual(store['fields']['rect'], {'x': 84, 'y': 1394, 'width': 366, 'scale': 1.0})
        # Gold, the 4-cell belt, the pouch and the native 366x92 Deposit button
        # all fit below grid row 8 without overlapping one another.
        rects = [(87, 1340, 317, 44), (467, 1345, 506, 112),
                 (1100, 1340, 280, 128), (84, 1394, 366, 92)]
        for i, (x, y, w, h) in enumerate(rects):
            self.assertGreaterEqual(y, 1333)
            self.assertLessEqual(x + w, 1408)
            self.assertLessEqual(y + h, 1486)
            for xx, yy, ww, hh in rects[i + 1:]:
                self.assertTrue(x + w <= xx or xx + ww <= x or y + h <= yy or yy + hh <= y)


    def test_hidden_controller_legends_stay_hidden(self):
        children = []
        for side in ('right', 'left'):
            for name in ('background_' + side + '_arm', 'background_' + side + '_arm_selected', 'WeaponSwap' + side.title() + 'Legend'):
                children.append({'name': name, 'fields': {'rect': {'x': 99999 if name.startswith('Weapon') else 215, 'y': 134}}})
        for name in ('text_i_left', 'text_ii_left', 'text_i_right', 'text_ii_right'):
            children.append({'name': name, 'fields': {'rect': {'x': 227, 'y': 131}}})
        result = json.loads(B.patch_layout(json.dumps({'children': children}), B.EXPANSION))
        for child in result['children']:
            if child['name'].startswith('Weapon'):
                self.assertEqual(child['fields']['rect']['x'], 99999)

    def test_disabled_never_reads_resources_or_writes(self):
        with patch.object(B, 'mode', return_value=0):
            self.assertEqual(B.build('nonexistent', 'nonexistent', {}), {})
            A.main('nonexistent', 'nonexistent')
            self.assertEqual(B.patch_bin(b''), b'')
            self.assertEqual(B.patch_txt(''), '')
            self.assertEqual(B.patch_layout(''), '')

    def test_block_addressing_is_bijective_and_bounded(self):
        data = sprite()
        w, h, addresses = A.blocks(data)
        self.assertEqual((w, h), (512, 700))
        self.assertEqual(len(set(addresses)), 128 * 175)
        self.assertEqual(addresses[:5], [40, 72, 296, 328, 8232])
        self.assertTrue(all(i % 16 == 8 and i + 16 <= len(data) for i in addresses))
        with self.assertRaises(ValueError):
            A.blocks(data[:1000])

    def test_full_grid_is_13_by_8_unscaled_source_tiles(self):
        image = Image.new('RGBA', (512, 700))
        # An asymmetric original cell exposes flips, scaling and pitch errors.
        for y in range(425, 474):
            for x in range(9, 58):
                image.putpixel((x, y), (x, y - 400, 128, 255))
        result = A.compose(image)
        self.assertEqual(result.size, (692, 792))
        tile = image.crop((9, 425, 58, 474)).tobytes()
        for row in range(8):
            for col in range(13):
                x, y = 27 + col * 49, 333 + row * 49
                self.assertEqual(result.crop((x, y, x + 49, y + 49)).tobytes(), tile)

    def test_mode1_preserves_non_grid_bc3_blocks(self):
        original = sprite()
        with patch.object(B, 'mode', return_value=1):
            result = A.generate(original)
        self.assertEqual(result[:40], original[:40])
        w, _, addresses = A.blocks(result)
        for i, at in enumerate(addresses):
            row, col = divmod(i, (w + 3) // 4)
            if col * 4 + 4 <= 9 or col * 4 >= 499 or row * 4 + 4 <= 425 or row * 4 >= 621:
                self.assertEqual(result[at:at + 16], original[at:at + 16])

    def test_large_sprite_header_and_decode(self):
        image = Image.new('RGBA', (692, 792), (255, 0, 0, 255))
        result = A.encode(image, sprite())
        self.assertEqual(struct.unpack_from('<HII', result, 6), (691, 692, 792))
        self.assertEqual(struct.unpack_from('<I', result, 32)[0], len(result) - 40)
        decoded = A.decode(result)
        self.assertEqual(decoded.size, image.size)
        self.assertEqual(decoded.getpixel((691, 791)), (255, 0, 0, 255))


if __name__ == '__main__':
    unittest.main()
