#if !defined(VIRTIO_H)
#define VIRTIO_H

#include <std.h>
#include <virtio_queue.h>

#define VIRTIO_BLK_BASE 0xc01000
#define VIRTIO_BLK_MMIO_OFFSET PAGE_SIZE

// Virtio Blk Feature Bits
#define VIRTIO_BLK_F_BLK_SIZE (1UL << 6)
#define VIRTIO_BLK_F_FLUSH (1UL << 9)

// Virtio Researved Feature Bits
#define VIRTIO_F_VERSION_1 (1UL << 32)
#define VIRTIO_F_IN_ORDER (1UL << 35)

#define VIRTIO_SUPPORTED_FEATURES ( \
    VIRTIO_BLK_F_BLK_SIZE | \
    VIRTIO_BLK_F_FLUSH | \
    VIRTIO_F_VERSION_1 | \
    VIRTIO_F_IN_ORDER \
)

#define VIRTIO_STATUS_RESET 0
#define VIRTIO_STATUS_ACKNOWLEDGE (1 << 0)
#define VIRTIO_STATUS_DRIVER (1 << 1)
#define VIRTIO_STATUS_DRIVER_OK (1 << 2)
#define VIRTIO_STATUS_FEATURES_OK (1 << 3)
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET (1 << 6)
#define VIRTIO_STATUS_FAILED (1 << 7)

#define VIRTIO_BLK_MAX_NUM_QUEUE 1
#define VIRTIO_BLK_SECTOR_SIZE 512

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

struct __attribute__((packed)) virtio_blk_config {
    le64 capacity;
    le32 size_max;
    le32 seg_max;
    struct virtio_blk_geometry {
        le16 cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    le32 blk_size;
    struct virtio_blk_topology {
        // # of logical blocks per physical block (log2)
        uint8_t physical_block_exp;
        // offset of first aligned logical block
        uint8_t alignment_offset;
        // suggested minimum I/O size in blocks
        le16 min_io_size;
        // optimal (suggested maximum) I/O size in blocks
        le32 opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0;
    uint16_t num_queues;
    le32 max_discard_sectors;
    le32 max_discard_seg;
    le32 discard_sector_alignment;
    le32 max_write_zeroes_sectors;
    le32 max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    le32 max_secure_erase_sectors;
    le32 max_secure_erase_seg;
    le32 secure_erase_sector_alignment;
};

struct virtio_mmio_device_registers {
    uint32_t magic_number;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint64_t device_features;
    uint32_t device_features_sel;
    uint64_t driver_features;
    uint32_t driver_features_sel;
    uint32_t queue_sel;
    uint32_t queue_num_max;
    uint32_t queue_notify;
    uint32_t interrupt_status;
    uint32_t interrupt_ack;
    uint32_t status;
    uint32_t shm_sel;
    uint32_t shm_len_low;
    uint32_t shm_len_high;
    uint32_t shm_base_low;
    uint32_t shm_base_high;
    uint32_t queue_reset;
    uint32_t config_generation;
};

struct virtioq_metadata {
    uint32_t queue_size;
    uint32_t queue_ready;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
};

typedef struct {
    struct virtio_mmio_device_registers regs;
    struct virtio_blk_config config;
    struct virtioq_metadata q_stat[VIRTIO_BLK_MAX_NUM_QUEUE];
} virtio_blk_device;

struct __attribute__((packed))  virtio_blk_req_hdr {
    le32 type;
    le32 reserved;
    le64 sector;
};

struct __attribute__((packed)) virtio_blk_req {
    struct virtio_blk_req_hdr hdr;
    uint8_t data[VIRTIO_BLK_SECTOR_SIZE];
    uint8_t status;
};

void virtio_handler(struct mmio_access *mmio, virtio_blk_device *dev);

uint32_t virtio_status_write_handler(uint32_t new_status, virtio_blk_device *dev);

void virtio_write_handler(struct mmio_access *mmio, virtio_blk_device *dev);

void virtio_read_handler(struct mmio_access *mmio, virtio_blk_device *dev);

int virtio_blk_req_handler(uint64_t data, virtio_blk_device *dev);

#endif // VIRTIO_H