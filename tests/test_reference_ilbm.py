import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("reference_ilbm", ROOT / "tools" / "reference_ilbm.py")
ref = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)


def chunk(tag, data):
    return tag + len(data).to_bytes(4, "big") + data + (bytes([0]) if len(data) & 1 else b"")


def sample(width=9, height=1, planes=2, masking=0, compression=0, body=None, camg=None):
    bmhd = (width.to_bytes(2, "big") + height.to_bytes(2, "big") +
            bytes(4) + bytes([planes, masking, compression, 0]) + bytes(8))
    stride = ((width + 15) // 16) * 2
    if body is None:
        body = bytes(stride * (planes + (masking == 1)) * height)
    payload = b"ILBM" + chunk(b"BMHD", bmhd)
    if camg is not None:
        payload += chunk(b"CAMG", camg.to_bytes(4, "big"))
    payload += chunk(b"BODY", body)
    return b"FORM" + len(payload).to_bytes(4, "big") + payload


class ReferenceDecoderTests(unittest.TestCase):
    def test_planar_pixels_and_padding(self):
        # 9 pixels: plane 0 bits at x=0,8; plane 1 bits at x=1,8.
        data = sample(body=bytes([0x80, 0x80, 0x40, 0x80]))
        self.assertEqual(ref.decode(data), (9, 1, bytes([1, 2, 0, 0, 0, 0, 0, 0, 3])))

    def test_mask_plane_is_ignored(self):
        data = sample(masking=1, body=bytes([0x80, 0, 0, 0, 0xff, 0xff]))
        self.assertEqual(ref.decode(data)[2][0], 1)

    def test_byterun1_literal_and_repeat(self):
        # Each row plane is two bytes: literal 0x80,0 then repeat 0,0.
        body = bytes([1, 0x80, 0, 0xff, 0])
        data = sample(compression=1, body=body)
        self.assertEqual(ref.decode(data)[2][0], 1)

    def test_byterun1_truncated(self):
        with self.assertRaises(ValueError):
            ref.decode(sample(compression=1, body=bytes([2, 0])))

    def test_byterun1_overflow(self):
        with self.assertRaises(ValueError):
            ref.decode(sample(compression=1, body=bytes([0x81, 0])))

    def test_ham_and_ehb_rejected(self):
        for mode in (0x800, 0x80):
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                ref.decode(sample(camg=mode))

    def test_bad_form_length(self):
        with self.assertRaises(ValueError):
            ref.decode(sample()[:-1])


if __name__ == "__main__":
    unittest.main()
