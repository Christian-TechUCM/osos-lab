/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef REPRISE_LAYOUT_H
#define REPRISE_LAYOUT_H
#include <stdint.h>
#include <stdbool.h>

struct disk_layout {
    int32_t rc[5];
    uint32_t mask, lba[4], partition_scale;
    uint32_t error, failed_partition;
    uint8_t sectors[5][512] __attribute__((aligned(32)));
};

enum layout_error {
    LAYOUT_OK, LAYOUT_DISK_SECTOR_SIZE, LAYOUT_MBR_READ, LAYOUT_MBR_SIGNATURE,
    LAYOUT_NO_FAT32, LAYOUT_MULTIPLE_FAT32, LAYOUT_BOOT_SECTOR_READ,
    LAYOUT_SECTOR_SIZE, LAYOUT_PARTITION_BOUNDS, LAYOUT_BOOT_SECTOR_LOCATION,
    LAYOUT_FAT_GEOMETRY, LAYOUT_FAT_SIGNATURE, LAYOUT_CLUSTER_COUNT,
    LAYOUT_FAT_CAPACITY, LAYOUT_ROOT_CLUSTER, LAYOUT_OTHER_PARTITION_BOUNDS,
    LAYOUT_PARTITION_OVERLAP,
    LAYOUT_CLUSTER_SIZE, LAYOUT_RESERVED_SECTORS, LAYOUT_FAT_COUNT,
    LAYOUT_FAT_SIZE, LAYOUT_VOLUME_BOUNDS, LAYOUT_DATA_REGION,
    LAYOUT_ROOT_ENTRIES, LAYOUT_FAT16_VOLUME, LAYOUT_FAT16_SIZE, LAYOUT_FAT_VERSION,
    LAYOUT_APPLE_PARTITION_MAP,
};

/* Optional USB request 0x56; all fields are little-endian 32-bit words. */
struct storage_diagnostics {
    uint32_t magic, version, sector_bytes, sectors_low, sectors_high, battery_mv;
    uint32_t layout_error, failed_partition, read_mask;
    int32_t read_rc[5];
    struct {
        uint32_t type, start, count, read_lba, sector_bytes, cluster_sectors;
        uint32_t reserved_sectors, fats, volume_sectors, fat_sectors, root_cluster, flags;
        uint32_t root_entries, volume_sectors16, fat_sectors16, version;
    } partitions[4];
};

_Static_assert(sizeof(struct storage_diagnostics) == 312, "storage diagnostics");
extern struct storage_diagnostics upload_diagnostics;

/* Optional requests 0x57 (header) and 0x58 (attempt selected by wValue). */
#define LAYOUT_ATTEMPTS 20
struct storage_probe {
    uint32_t magic, version, count, flags, dma_mode, pio_time;
    uint32_t logical_bytes, physical_bytes;
    char model[40], revision[8];
    uint32_t identify106, identify117, identify118, driver_phys_mult;
};

enum probe_result {
    PROBE_HEADER_OK, PROBE_READ_FAILED, PROBE_REPEAT_FAILED, PROBE_MISMATCH,
    PROBE_SIGNATURE, PROBE_SECTOR_SIZE, PROBE_BOUNDS, PROBE_PARTITION_HEADER,
    PROBE_NESTED_MBR, PROBE_NESTED_BOOT,
};

struct layout_attempt {
    uint32_t slot, multiplier, lba;
    int32_t rc, repeat_rc;
    uint32_t matches, result, lba_high;
    uint8_t sectors[2][512] __attribute__((aligned(32)));
};

_Static_assert(sizeof(struct storage_probe) == 96, "storage probe header");
_Static_assert(sizeof(struct layout_attempt) == 1056, "storage probe attempt");
extern struct storage_probe upload_probe;
extern struct layout_attempt upload_attempts[LAYOUT_ATTEMPTS];

static inline uint32_t le32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool file_layout_ok(struct disk_layout *layout, uint64_t total, uint64_t *start,
                    uint64_t *end, unsigned *bytes);
#endif
