#if !defined(STD_H)
#define STD_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#define BYTE_SWAP_U32(x) ( \
    ((x & 0xff) << 24) | \
    ((x & 0xff00) << 8) | \
    ((x & 0xff0000) >> 8) | \
    ((x & 0xff000000) >> 24) \
)

#define BYTE_SWAP_U64(x) ( \
    ((x & 0xff) << 56) | \
    ((x & 0xff00) << 40) | \
    ((x & 0xff0000) << 24) | \
    ((x & 0xff000000) << 8) | \
    ((x & 0xff00000000) >> 8) | \
    ((x & 0xff0000000000) >> 24) | \
    ((x & 0xff000000000000) >> 40) | \
    ((x & 0xff00000000000000) >> 56) \
)

#define KVM_FILE "/dev/kvm"
#define PAGE_SIZE 0x1000       // define 4 KB page size

#define NORM_MEM_BASE 0x00000
#define NORM_MEM_SIZE 0xc00000

#define MMIO_BASE 0xc00000
#define MMIO_SIZE 0x400000

#define VGIC_DIST_REGS_BASE 0x1000000
#define VGIC_DIST_REGS_SIZE 0x1000
#define VGIC_CPU_REGS_BASE (VGIC_DIST_REGS_BASE + VGIC_DIST_REGS_SIZE)
#define VGIC_CPU_REGS_SIZE 0x2000

// ARM GICv2 GICD register offsets
enum {
    GICD_CTLR,
    GICD_TYPER,
    GICD_IIDR,
    GICD_IGROUPR0,
    GICD_ISENABLER0,
    GICD_ICENABLER0,
    GICD_ISPENDR0,
    GICD_ICPENDR0,
    GICD_ISACTIVER0,
    GICD_ICACTIVER0,
    GICD_IPRIORITYR0,
    GICD_ITARGETRR0,
    GICD_ICFGR0,
    GICD_NSACR0,
    GICD_SGIR,
    GICD_CPENDSGIR0,
    GICD_SPENDSGIR0,
};

// ARM GICv2 GICC register offsets
enum {
    GICC_CTLR,
    GICC_PMR,
    GICC_BPR,
    GICC_IAR,
    GICC_,
    GICC_EOIR,
    GICC_RPR,
    GICC_HIPPR,
    GICC_ABPR,
    GICC_AIAR,
    GICC_AEOIR,
    GICC_AHPPIR,
    GICC_APR0,
    GICC_NSAPR0,
    GICC_IIDR,
    GICC_DIR,
};

struct list_entry {
    int fd;
    SLIST_ENTRY(list_entry) entries;
};

struct vm {
    int fd;
    SLIST_HEAD(vcpu_list_head, list_entry);
    SLIST_HEAD(device_list_head, list_entry);
};

struct mmio_access {
    uint64_t phys_addr;
    uint8_t data[8];
    uint32_t len;
    uint8_t is_write;
};

extern void *normal_mem;

extern int vm_fd;

extern struct kvm_create_device arm_vgic;

void get_vgic_attr(int fd, uint32_t group, uint32_t attr, uint64_t addr);

#endif // STD_H