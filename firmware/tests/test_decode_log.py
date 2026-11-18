"""Regression vectors for the receiver-side default-profile log decoder."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "can"))
from decode_joystick_can import decode, parse_line


class DecoderTests(unittest.TestCase):
    def test_supported_log_formats(self):
        for line in ("1500#010D002280007FFF", "0x5DC#010D002280007FFF",
                     "(0.123456) can0 5DC#010D002280007FFF",
                     "can0 1500#010D002280007FFF",
                     "can0 5DC [8] 01 0D 00 22 80 00 7F FF"):
            with self.subTest(line=line):
                frame = parse_line(line)
                self.assertEqual(frame.can_id, 1500)
                self.assertIn("X=32768 Y=32767", decode(frame))

    def test_socketcan_three_numeric_digits_are_hex(self):
        self.assertEqual(parse_line("can0 600#0000000000000000").can_id, 0x600)

    def test_rejects_dlc_trailing_bytes_and_extended_ids(self):
        for line in ("can0 5DC [7] 01 0D 00 22 80 00 7F FF",
                     "can0 5DC [8] 01 0D 00 22 80 00 7F FF 99",
                     "can0 0800#010D002280007FFF", "can0 7F#010D002280007FFF",
                     "can0 000005DC#010D002280007FFF", "0xFFF#010D002280007FFF"):
            self.assertIsNone(parse_line(line))

    def test_button_contract_and_status_are_distinct(self):
        self.assertIn("buttons=0x80000001", decode(parse_line("1504#0105000080000001")))
        mismatch = decode(parse_line("1504#0110000080000001"))
        self.assertIn("INVALID_TYPE", mismatch)
        self.assertNotIn("hid_sample", mismatch)
        self.assertIn("hid_sample=1", decode(parse_line("1506#0110000000010600")))
        self.assertNotIn("hid_sample", decode(parse_line("1600#0110000000010600")))


if __name__ == "__main__":
    unittest.main()
