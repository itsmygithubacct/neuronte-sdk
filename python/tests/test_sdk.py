"""Cell-surface tests. No terminal, no model, no GPU."""

from __future__ import annotations

import ctypes
import unittest

import neuronte_sdk as nsdk


def one_hue(pixel: int, color: int) -> bool:
    if (pixel >> 24) & 0xFF != 0xFF:
        return False
    channels = [(pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF]
    sources = [(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF]
    if any(got > src for got, src in zip(channels, sources)):
        return False
    for index in range(3):
        other = (index + 1) % 3
        cross = channels[index] * sources[other] - channels[other] * sources[index]
        limit = (sources[index] + sources[other]) // 2 + 1
        if abs(cross) > limit:
            return False
    return True


class ApiTests(unittest.TestCase):
    def test_net_names_constant(self) -> None:
        self.assertEqual(
            nsdk.NET_NAMES,
            ("ascii", "draw", "symbol", "accent", "pixel", "braille"),
        )

    def test_explicit_missing_weights_do_not_open(self) -> None:
        with self.assertRaises(nsdk.NeuronteError) as caught:
            nsdk.open("no-such-neuronte-weights.nrt")
        self.assertNotEqual(caught.exception.code, nsdk.NEURONTE_OK)


class FaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        try:
            cls.face = nsdk.open()
        except nsdk.NeuronteError as exc:
            if exc.code == nsdk.NEURONTE_ERR_IO:
                raise unittest.SkipTest(str(exc)) from exc
            raise

    @classmethod
    def tearDownClass(cls) -> None:
        face = getattr(cls, "face", None)
        if face is not None:
            face.close()

    def test_split_v4_routes(self) -> None:
        face = self.face
        self.assertEqual(face.nets(), 6)
        rows = (
            ("ascii", 0, (ord(" "), ord("A"), ord("#"))),
            ("draw", 1, (0x2500, 0x2502, 0x250C, 0x2591)),
            ("symbol", 2, (0x2192,)),
            ("accent", 3, (0x00E9,)),
            ("pixel", 4, (0x2588, 0x2580)),
            ("braille", 5, (0x2800,)),
        )
        for name, index, scalars in rows:
            with self.subTest(net=name):
                for scalar in scalars:
                    self.assertEqual(face.route(scalar), index)
                self.assertEqual(face.net_name(index), name)
        with self.assertRaises(nsdk.NeuronteError):
            face.net_name(6)

    def test_draw_a_is_one_hue(self) -> None:
        width, height = 8, 16
        color = 0xFF66CCFF
        pixels = (ctypes.c_uint32 * (width * height))()
        for index in range(width * height):
            pixels[index] = 0xFF000000
        self.face.draw_scalar(
            pixels, width, width, height, 0, 0, width, height, ord("A"), color
        )
        changed = [int(pixel) for pixel in pixels if int(pixel) != 0xFF000000]
        self.assertGreater(len(changed), 0)
        for pixel in changed:
            self.assertTrue(one_hue(pixel, color), hex(pixel))

    def test_grid_text_hi_records_ascii(self) -> None:
        color = 0xFF66CCFF
        grid = nsdk.Grid(self.face, 8, 4)
        grid.text(1, 1, "Hi", color)
        records = list(grid.records())
        self.assertEqual(len(records), 8 * 4)
        row = [record for record in records if record["row"] == 1]
        self.assertEqual(row[1]["scalar"], ord("H"))
        self.assertEqual(row[2]["scalar"], ord("i"))
        self.assertEqual(row[1]["col"], 1)
        self.assertEqual(row[2]["col"], 2)
        for record in row[1:3]:
            self.assertEqual(record["route"], 0)
            self.assertEqual(record["net"], "ascii")
            self.assertEqual(record["color"], color)
            self.assertEqual(record["route"], self.face.route(record["scalar"]))
            self.assertEqual(record["net"], self.face.net_name(record["route"]))
        self.assertEqual(row[0]["scalar"], 0)
        self.assertIn("net", row[0])

    def test_named_cells(self) -> None:
        grid = nsdk.Grid(self.face, 4, 2)
        grid.put(0, 0, 0x2500, 0xFFFFFFFF)
        grid.put(1, 0, 0x2588, 0xFFFFFFFF)
        grid.put(2, 0, 0x2192, 0xFFFFFFFF)
        found = {(record["col"], record["row"]): record for record in grid.records()}
        self.assertEqual(found[(0, 0)]["net"], "draw")
        self.assertEqual(found[(0, 0)]["route"], 1)
        self.assertEqual(found[(1, 0)]["net"], "pixel")
        self.assertEqual(found[(1, 0)]["route"], 4)
        self.assertEqual(found[(2, 0)]["net"], "symbol")
        self.assertEqual(found[(2, 0)]["route"], 2)
        self.assertEqual(len(list(grid.records())), 4 * 2)

    def test_rgb_length(self) -> None:
        grid = nsdk.Grid(self.face, 3, 2, cell_w=8, cell_h=16)
        rgb = grid.rgb()
        self.assertIsInstance(rgb, bytes)
        self.assertEqual(len(rgb), 3 * 8 * 2 * 16 * 3)
        self.assertEqual(rgb[:3], bytes((0x10, 0x10, 0x10)))
        view = grid.pixels()
        self.assertIsInstance(view, memoryview)
        self.assertEqual(len(view), grid.width * grid.height)
        self.assertEqual(int(view[0]), 0xFF101010)

    def test_default_cell_rgb_length(self) -> None:
        grid = nsdk.Grid(self.face, 2, 2)
        self.assertEqual(len(grid.rgb()), 2 * 8 * 2 * 16 * 3)

    def test_closed_face_rejects_draws(self) -> None:
        face = nsdk.open()
        face.close()
        face.close()
        with self.assertRaises(nsdk.FaceClosedError):
            face.nets()


if __name__ == "__main__":
    unittest.main()
