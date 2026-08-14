#include <stdio.h>
#include <string.h>
#include <virtio.h>

void virtio_handler(struct mmio_access *mmio, virtio_blk_device *dev) {
    if (mmio->is_write) {
        virtio_write_handler(mmio, dev);
    } else {
        virtio_read_handler(mmio, dev);
    }
}

uint32_t virtio_status_write_handler(uint32_t new_status, virtio_blk_device *dev) {
    uint32_t set_bit = new_status & ~dev->regs.status;
    uint32_t ret = new_status;
    switch (set_bit) {
        case VIRTIO_STATUS_RESET:
            printf("Virtio device reset triggered.\n");
            ret = 0;
            break;
        case VIRTIO_STATUS_ACKNOWLEDGE:
            printf("Virtio device acknowledged by guest OS.\n");
            break;
        case VIRTIO_STATUS_DRIVER:
            printf("Guest OS knows how to drive virtio device.\n");
            break;
        case VIRTIO_STATUS_FEATURES_OK:
            printf("FEATURES_OK bit set.\n");
            printf("Device offered features: %llX.\n", dev->regs.device_features);
            printf("Driver accepted features: %llX.\n", dev->regs.driver_features);
            if (dev->regs.driver_features & ~dev->regs.device_features) {
                printf("Driver accepted features not valid. ");
                printf("Clearing FEATURES_OK bit.\n");
                ret &= ~set_bit;
            } else {
                printf("Virtio feature negotiation complete.\n");
            }
            break;
        case VIRTIO_STATUS_DRIVER_OK:
            printf("Virtio driver OK.\n");
            break;
        case VIRTIO_STATUS_DEVICE_NEEDS_RESET:
            printf("Virtio device needs reset.\n");
            break;
        case VIRTIO_STATUS_FAILED:
            printf("Virtio device status failed set from guest OS.\n");
            break;
        default:
            printf("Unkown virtio status bit set.\n");
    }
    return ret;
}

// TODO: Handle incorrect writes
void virtio_write_handler(struct mmio_access *mmio, virtio_blk_device *dev) {
    uint32_t offset = mmio->phys_addr - VIRTIO_BLK_BASE;
    uint64_t data = 0;
    memcpy(&data, mmio->data, mmio->len);
    printf("Virtio Write Handler triggered at offset 0x%x with value 0x%x\n", offset, data);

    uint8_t *dest;

    switch (offset) {
        case 0x14:
            dest = &dev->regs.device_features_sel; 
            break;
        case 0x20:
            uint32_t features_sel = dev->regs.driver_features_sel;
            // Convert driver features into a 32 bit array and select lower or 
            // upper half based upon features_sel and again convert back to 
            // uint8_t pointer. Because ARM64 is Little Endian, 0 index,
            // will read the lower 4 bytes.
            dest = (uint8_t *)&((uint32_t *)&dev->regs.driver_features)[features_sel];
            break;
        case 0x24:
            dest = &dev->regs.driver_features_sel;
            break;
        case 0x30:
            dest = &dev->regs.queue_sel;
            break;
        // Handle cases where driver tries to write into unavailable queue
        case 0x34:
            break;
        case 0x70:
            data = virtio_status_write_handler(data, dev);
            dest = &dev->regs.status;
            break;
        default:
            printf("Found virtio blk MMIO write at invalid offset.");
            printf(" Doing nothing.\n"); 
            return;
    };
    memcpy(dest, &data, mmio->len);
}

void virtio_read_handler(struct mmio_access *mmio, virtio_blk_device *dev) {
    uint32_t offset = mmio->phys_addr - VIRTIO_BLK_BASE;
    printf("Virtio Read Handler triggered at offset 0x%x.\n", offset);

    if (offset >= 0x100) {
        uint32_t config_offset = offset - 0x100;
        if (config_offset + mmio->len > sizeof(dev->config)) {
            printf("Virtio Blk Config read out of bounds.\n");
            memset(mmio->data, 0, mmio->len);
            return;
        }
        memcpy(mmio->data, (uint8_t*)&dev->config + config_offset, mmio->len);
        return;
    }

    uint32_t read_val = 0;

    switch (offset) {
        case 0x00:
            read_val = dev->regs.magic_number;
            break;
        case 0x04:
            read_val = dev->regs.version;
            break;
        case 0x08:
            read_val = dev->regs.device_id;
            break;
        case 0xc:
            read_val = dev->regs.vendor_id;
            break;
        case 0x10:
            // Device Features is a 64 bit register
            // If device features select bit is 0, we show the lower 32 bits
            // If it is 1, we show the upper 32 bits.
            uint32_t features_sel = dev->regs.device_features_sel;
            uint64_t device_features = dev->regs.device_features; 
            read_val = (device_features >> (features_sel * 32)) & 0xffffffff;
            break;
        case 0x70:
            read_val = dev->regs.status;
            break;
        default:
            printf("Found virtio blk MMIO read at unknown offset.");
            printf(" Doing nothing.\n"); 
            return;
    }
    printf("Sending value 0x%x.\n", read_val);
    memcpy(mmio->data, &read_val, sizeof(uint32_t));
}