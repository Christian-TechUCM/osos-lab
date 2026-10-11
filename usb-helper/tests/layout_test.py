# SPDX-License-Identifier: GPL-3.0-only
"""Offline partition and FAT32 boundary checks."""

import ctypes
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]


class LayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name)
        main = (HERE / "main.c").read_text()
        reader = main[
            main.index("static struct layout_attempt *new_attempt") : main.index(
                "static void save_ata_diagnostics"
            )
        ]
        (root / "layout.c").write_text(
            """
#include "layout.h"
#include <string.h>
static struct disk_layout layout;
struct storage_probe upload_probe;
struct layout_attempt upload_attempts[LAYOUT_ATTEMPTS];
static struct { uint32_t sector_size, sectors_low, sectors_high; } record;
#define RECORD (&record)
#define IF_MD(a, b)
static const uint8_t *disk_mbr, *disk_bpb;
static uint64_t boot_lba;
static unsigned partition_reads;
static int read_error;
static uint64_t accepted_start, accepted_end;
static unsigned accepted_bytes;
static int ata_read_sectors(uint32_t lba, unsigned count, void *out) {
    (void)count;
    if (lba == 0) {
        memcpy(out, disk_mbr, 512);
        return 0;
    }
    ++partition_reads;
    if (read_error && read_error != -901 && read_error != -902)
        return read_error;
    if (read_error == -901 && !(partition_reads & 1))
        return -9;
    if (lba == boot_lba)
        memcpy(out, disk_bpb, 512);
    else
        memset(out, 0, 512);
    if (read_error == -902 && !(partition_reads & 1))
        ((uint8_t *)out)[100] ^= 1;
    return 0;
}
"""
            + reader
            + """
int read_validate(const uint8_t *mbr, const uint8_t *bpb, uint64_t capacity,
                  unsigned slot, int error, unsigned scale) {
    record.sector_size = 512;
    record.sectors_low = (uint32_t)capacity;
    record.sectors_high = (uint32_t)(capacity >> 32);
    disk_mbr = mbr;
    disk_bpb = bpb;
    boot_lba = (uint64_t)le32(mbr+454+slot*16) * scale;
    read_error = error;
    partition_reads = 0;
    read_layout();
    return file_layout_ok(&layout, capacity, &accepted_start, &accepted_end,
                          &accepted_bytes);
}
unsigned reads(void) { return partition_reads; }
unsigned attempts(void) { return upload_probe.count; }
void attempt(unsigned i, void *out) { memcpy(out, &upload_attempts[i], sizeof(upload_attempts[i])); }
int partition_read_rc(unsigned slot) { return layout.rc[slot+1]; }
uint64_t selected_start(void) { return accepted_start; }
uint64_t selected_end(void) { return accepted_end; }
unsigned selected_bytes(void) { return accepted_bytes; }
int validate(const uint8_t *mbr, const uint8_t *bpb, uint64_t capacity,
             unsigned slot, unsigned scale) {
    memset(&layout, 0, sizeof(layout));
    memcpy(layout.sectors[0], mbr, 512);
    memcpy(layout.sectors[slot+1], bpb, 512);
    layout.mask = 1 | (1u << (slot+1));
    layout.partition_scale = scale;
    layout.lba[slot] = le32(mbr+454+slot*16) * scale;
    uint64_t start, end; unsigned bytes;
    return file_layout_ok(&layout, capacity, &start, &end, &bytes);
}
unsigned rejection(void) { return layout.error; }
unsigned rejected_partition(void) { return layout.failed_partition; }
"""
        )
        library = root / "layout.so"
        subprocess.run(
            [
                "cc",
                "-shared",
                "-fPIC",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(HERE),
                str(HERE / "layout.c"),
                str(root / "layout.c"),
                "-o",
                str(library),
            ],
            check=True,
        )
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.validate.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_uint64,
            ctypes.c_uint,
            ctypes.c_uint,
        ]
        cls.lib.read_validate.argtypes = cls.lib.validate.argtypes[:-1] + [
            ctypes.c_int,
            ctypes.c_uint,
        ]
        cls.lib.selected_start.restype = ctypes.c_uint64
        cls.lib.selected_end.restype = ctypes.c_uint64

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def image(self, bytes_per_sector=512, slot=0):
        mbr, bpb = bytearray(512), bytearray(512)
        mbr[510:] = bpb[510:] = b"\x55\xaa"
        mbr[450 + slot * 16] = 0x0C
        struct.pack_into("<II", mbr, 454 + slot * 16, 2048, 2000000)
        struct.pack_into("<HBHB", bpb, 11, bytes_per_sector, 4, 32, 2)
        struct.pack_into("<II", bpb, 32, 2000000, 4000)
        struct.pack_into("<I", bpb, 44, 2)
        return mbr, bpb

    def valid(self, mbr, bpb, slot=0, capacity=20000000, scale=None):
        if scale is None:
            scale = struct.unpack_from("<H", bpb, 11)[0] // 512
        return bool(self.lib.validate(bytes(mbr), bytes(bpb), capacity, slot, scale))

    def discover(self, mbr, bpb, slot=0, capacity=20000000, error=0, scale=None):
        if scale is None:
            scale = struct.unpack_from("<H", bpb, 11)[0] // 512
        return bool(
            self.lib.read_validate(bytes(mbr), bytes(bpb), capacity, slot, error, scale)
        )

    def attempt(self, index):
        out = ctypes.create_string_buffer(1056)
        self.lib.attempt(index, out)
        return struct.unpack_from("<IIIiiIII", out), out.raw[32:544], out.raw[544:]

    def test_preserves_each_candidate_and_both_reads(self):
        mbr, bpb = self.image()
        self.assertTrue(self.discover(mbr, bpb, scale=8))
        self.assertEqual(self.lib.attempts(), 5)
        fields, first, repeat = self.attempt(0)
        self.assertEqual(fields[:7], (0, 1, 0, 0, 0, 1, 0))
        self.assertEqual(first, mbr)
        self.assertEqual(repeat, mbr)
        for index, mult in enumerate([1, 2, 4, 8], 1):
            fields, first, repeat = self.attempt(index)
            self.assertEqual(fields[:6], (1, mult, 2048 * mult, 0, 0, 1))
            self.assertEqual(fields[6], 0 if mult == 8 else 4)
            self.assertEqual(first, bpb if mult == 8 else bytes(512))
            self.assertEqual(first, repeat)

    def test_records_read_errors_and_mismatches_without_accepting_them(self):
        for fault, expected in [
            (-9, (-9, -200, 0, 1)),
            (-901, (0, -9, 0, 2)),
            (-902, (0, 0, 0, 3)),
        ]:
            self.assertFalse(self.discover(*self.image(), error=fault, scale=1))
            fields, first, repeat = self.attempt(1)
            self.assertEqual(fields[3:7], expected)
            if fault == -902:
                self.assertNotEqual(first, repeat)
            elif fault == -901:
                self.assertEqual(repeat, bytes(512))

    def test_records_skipped_bounds_without_reading(self):
        mbr, bpb = self.image()
        self.assertFalse(self.discover(mbr, bpb, capacity=3000000, scale=8))
        self.assertEqual(self.lib.attempts(), 5)
        self.assertEqual(self.lib.reads(), 2)
        for index in [2, 3, 4]:
            fields, first, repeat = self.attempt(index)
            self.assertEqual(fields[3:7], (-200, -200, 0, 6))
            self.assertEqual(first + repeat, bytes(1024))

    def test_all_four_partition_candidates_fit_capture_buffer(self):
        mbr, bpb = self.image()
        for slot in range(4):
            mbr[450 + slot * 16] = 0x0C
            struct.pack_into("<II", mbr, 454 + slot * 16, 2048 * (slot + 1), 2000000)
        self.assertFalse(self.discover(mbr, bpb, slot=3, scale=8))
        self.assertEqual(self.lib.attempts(), 20)
        bpb[510:] = bytes(2)
        self.assertFalse(self.discover(mbr, bpb, scale=8))
        self.assertEqual(self.lib.attempts(), 17)
        self.assertEqual(self.attempt(16)[0][:3], (4, 8, 65536))

    def test_multiple_sector_sizes_and_partition_slots(self):
        for size in [512, 1024, 2048, 4096]:
            for slot in range(4):
                self.assertTrue(self.valid(*self.image(size, slot), slot=slot))

    def test_rejects_overlap_capacity_and_invalid_fat_geometry(self):
        mbr, bpb = self.image()
        self.assertFalse(self.valid(mbr, bpb, capacity=1000000))
        self.assertEqual(self.lib.rejection(), 8)
        self.assertEqual(self.lib.rejected_partition(), 1)
        mbr[466] = 0x3F
        struct.pack_into("<II", mbr, 470, 2040, 16)
        self.assertFalse(self.valid(mbr, bpb))
        self.assertEqual(self.lib.rejection(), 16)
        for offset, value, reason in [
            (13, 3, 17),
            (16, 0, 19),
            (17, 1, 23),
            (22, 1, 25),
            (42, 1, 26),
            (510, 0, 11),
        ]:
            mbr, bpb = self.image()
            bpb[offset] = value
            self.assertFalse(self.valid(mbr, bpb), offset)
            self.assertEqual(self.lib.rejection(), reason, offset)
        for offset, value in [(32, 3000000), (36, 1), (44, 1), (44, 3000000)]:
            mbr, bpb = self.image()
            struct.pack_into("<I", bpb, offset, value)
            self.assertFalse(self.valid(mbr, bpb), offset)

    def test_distinguishes_missing_fat32_from_invalid_boot_sector(self):
        mbr, bpb = self.image()
        mbr[450] = 0xAF
        self.assertFalse(self.valid(mbr, bpb))
        self.assertEqual(self.lib.rejection(), 4)
        mbr, bpb = self.image()
        bpb[510] = 0
        self.assertFalse(self.valid(mbr, bpb))
        self.assertEqual(self.lib.rejection(), 11)

    def test_discovery_preserves_partition_bounds_failure_without_reading(self):
        for slot in range(4):
            for start, count in [
                (2048, 2000000),
                (0, 100000),
                (2048, 0),
                (0xFFFFFF00, 0x1000),
            ]:
                with self.subTest(slot=slot, start=start, count=count):
                    mbr, bpb = self.image(slot=slot)
                    struct.pack_into("<II", mbr, 454 + slot * 16, start, count)
                    self.assertFalse(
                        self.discover(mbr, bpb, slot=slot, capacity=1000000)
                    )
                    self.assertEqual(self.lib.rejection(), 8)
                    self.assertEqual(self.lib.rejected_partition(), slot + 1)
                    self.assertEqual(self.lib.reads(), 0)
                    self.assertEqual(self.lib.partition_read_rc(slot), -200)

    def test_discovery_accepts_sector_scales_and_distinguishes_read_failure(self):
        for size in [512, 1024, 2048, 4096]:
            for slot in range(4):
                with self.subTest(size=size, slot=slot):
                    mbr, bpb = self.image(size, slot)
                    self.assertTrue(self.discover(mbr, bpb, slot=slot))
                    self.assertFalse(self.discover(mbr, bpb, slot=slot, error=-1))
                    self.assertEqual(self.lib.rejection(), 6)
                    self.assertEqual(self.lib.rejected_partition(), slot + 1)
                    self.assertGreater(self.lib.reads(), 0)
                    self.assertEqual(self.lib.partition_read_rc(slot), -1)

    def test_discovery_accepts_independent_partition_and_fat_sector_sizes(self):
        for scale in [1, 2, 4, 8]:
            for size in [512, 1024, 2048, 4096]:
                for slot in range(4):
                    with self.subTest(scale=scale, size=size, slot=slot):
                        mbr, bpb = self.image(size, slot)
                        count = 2000000 * (size // 512) // scale
                        struct.pack_into("<I", mbr, 458 + slot * 16, count)
                        start = 2048 * scale
                        end = start + count * scale
                        self.assertTrue(
                            self.discover(
                                mbr, bpb, slot=slot, scale=scale, capacity=end
                            )
                        )
                        self.assertEqual(self.lib.selected_start(), start)
                        self.assertEqual(self.lib.selected_end(), end)
                        self.assertEqual(self.lib.selected_bytes(), size)

    def test_mixed_units_reject_volume_overflow_and_partition_overlap(self):
        for scale, size in [(1, 4096), (8, 512)]:
            with self.subTest(scale=scale, size=size):
                mbr, bpb = self.image(size)
                count = 2000000 * (size // 512) // scale
                struct.pack_into("<I", mbr, 458, count - 1)
                self.assertFalse(self.discover(mbr, bpb, scale=scale))
                self.assertEqual(self.lib.rejection(), 21)
                struct.pack_into("<I", mbr, 458, count)
                mbr[466] = 0x3F
                struct.pack_into("<II", mbr, 470, 2040, 16)
                self.assertFalse(self.discover(mbr, bpb, scale=scale))
                self.assertEqual(self.lib.rejection(), 16)
                struct.pack_into("<II", mbr, 470, 63, 1985)
                self.assertTrue(self.discover(mbr, bpb, scale=scale))

    def test_rejects_invalid_partition_scales_and_zero_headers(self):
        mbr, bpb = self.image()
        for scale in [0, 3, 16]:
            self.assertFalse(self.valid(mbr, bpb, scale=scale))
            self.assertEqual(self.lib.rejection(), 9)
        self.assertFalse(self.discover(mbr, bytearray(512), scale=8))
        self.assertEqual(self.lib.rejection(), 7)

    def test_mixed_units_reject_partition_beyond_capacity(self):
        mbr, bpb = self.image(4096)
        struct.pack_into("<I", mbr, 458, 16000000)
        self.assertFalse(self.valid(mbr, bpb, scale=1, capacity=16002047))
        self.assertEqual(self.lib.rejection(), 8)
