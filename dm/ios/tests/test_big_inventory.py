"""Bag geometry regressions without proprietary game files or a device."""
import json
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import big_inventory as B
import gen_biginv_art as A


def table():
    data = bytearray(7204)
    struct.pack_into('<I', data, 0, 30)
    for row in B.PLAYER_ROWS:
        offset = 4 + row * 240
        data[offset + 16:offset + 18] = bytes((10, 4))
        struct.pack_into('<4i', data, offset + 20,
                         *((339, 626, 255, 368) if row < 15 else (419, 706, 315, 428)))
        data[offset + 36:offset + 38] = bytes((29, 29))
    # Existing stash/merc changes must survive composition.
    data[4 + 10 * 240 + 16] = 14
    data[4 + 11 * 240 + 120] = 42
    return bytes(data)


class BigInventoryTests(unittest.TestCase):
    def test_all_player_rows_only_four_bytes_each(self):
        original = table()
        result = B.patch_bin(original)
        changed = {i for i, (a, b) in enumerate(zip(original, result)) if a != b}
        expected = {4 + row * 240 + offset for row in B.PLAYER_ROWS for offset in (16, 17, 36, 37)}
        self.assertEqual(changed, expected)
        self.assertEqual(len(result), len(original))

    def test_fail_closed_on_changed_or_already_patched_input(self):
        with self.assertRaises(ValueError):
            B.patch_bin(B.patch_bin(table()))
        with self.assertRaises(ValueError):
            B.patch_bin(table()[:-1])

    def test_text_only_player_rows_and_keep_endpoints(self):
        header = 'class\tgridX\tgridY\tgridBoxWidth\tgridBoxHeight\tgridLeft\r\n'
        names = B.CLASSES + tuple(c + '2' for c in B.CLASSES)
        other = 'Trade Page 1\t10\t4\t29\t29\t20\r\n'
        text = header + ''.join(n + '\t10\t4\t29\t29\t339\r\n' for n in names) + other
        result = B.patch_txt(text)
        self.assertTrue(result.endswith(other))
        self.assertEqual(result.count('\t15\t6\t19\t19\t339\r\n'), 16)

    def test_layout_keeps_footprint_and_other_widgets(self):
        obj = {'basedOn': 'PlayerInventoryOriginalLayoutHD.json', 'children': [
            {'type': 'InventoryGridWidget', 'name': 'grid', 'fields': {'rect': {'x': 216, 'y': 893}, 'navigation': {'up': 'belt'}}},
            {'type': 'InventorySlotWidget', 'name': 'slot_head', 'fields': {'cellSize': '$ItemCellSize'}}]}
        result = json.loads(B.patch_layout(json.dumps(obj)))
        fields = result['children'][0]['fields']
        self.assertEqual(fields['rect'], obj['children'][0]['fields']['rect'])
        self.assertAlmostEqual(fields['cellSize']['x'] * fields['cellCount']['x'], 980)
        self.assertAlmostEqual(fields['cellSize']['y'] * fields['cellCount']['y'], 392)
        self.assertEqual(result['children'][1], obj['children'][1])
        self.assertEqual(fields['navigation'], obj['children'][0]['fields']['navigation'])

    def test_disabled_never_reads_resources_or_writes(self):
        with patch.object(B, 'enabled', return_value=False):
            self.assertEqual(B.build('nonexistent', 'nonexistent', {}), {})
            A.main('nonexistent', 'nonexistent')
        with patch.object(B.CONFIG.__class__, 'read_text', return_value='#define BIG_INV 0\n'):
            self.assertFalse(B.enabled())
        with patch.object(B.CONFIG.__class__, 'read_text', return_value='#define BIG_INV 1\n'):
            self.assertTrue(B.enabled())

    def test_block_addressing_is_bijective_and_bounded(self):
        data = bytearray(524328)
        data[:6] = b'SpA1\x3d\x00'
        struct.pack_into('<II', data, 8, 512, 700)
        struct.pack_into('<H', data, 20, 1)
        data[22:24] = bytes((3, 4))
        w, h, addresses = A.blocks(data)
        self.assertEqual((w, h), (512, 700))
        self.assertEqual(len(set(addresses)), 128 * 175)
        self.assertEqual(addresses[:5], [40, 72, 296, 328, 8232])
        self.assertTrue(all(i % 16 == 8 and i + 16 <= len(data) for i in addresses))

    def test_retile_does_not_touch_pixels_outside_bag(self):
        from PIL import Image
        image = Image.new('RGBA', (512, 700), (32, 64, 96, 255))
        result = A.retile(image)
        self.assertEqual(result.size, image.size)
        self.assertEqual(result.crop((0, 0, 512, 425)).tobytes(), image.crop((0, 0, 512, 425)).tobytes())
        self.assertEqual(result.crop((0, 621, 512, 700)).tobytes(), image.crop((0, 621, 512, 700)).tobytes())


if __name__ == '__main__':
    unittest.main()
