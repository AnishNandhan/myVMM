#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>

#define KVM_FILE "/dev/kvm"
#define PAGE_SIZE 0x1000       // define 4 KB page size

#define NORM_MEM_BASE 0x00000
#define NORM_MEM_SIZE 0xc00000

#define MMIO_BASE 0xc00000
#define MMIO_SIZE 0x400000

#define UART_16550_BASE MMIO_BASE + 0x000000

// 16550 UART Register Map
#define RBR_16550 0x0   // Receiver Buffer Register
#define THR_16550 0x0   // Transmitter Holding Register
#define IER_16550 0x1   // Interrupt Enable Register
#define IIR_16550 0x2   // Interrupt Identification Register
#define FCR_16550 0x2   // FIFO Control Register
#define LCR_16550 0x3   // Line Control Register
#define MCR_16550 0x4   // MODEM Control Register
#define LSR_16550 0x5   // Line Status Register
#define MSR_16550 0x6   // MODEM Status Register
#define SCR_16550 0x7   // Scratch Register
#define DLL_16550 0x0   // Divisor Latch (LS)
#define DLM_16550 0x1   // Divisor Latch (LM)

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

void uart_handler(struct mmio_access *mmio) {
    if (mmio->is_write) {
        write(STDOUT_FILENO, (void*)mmio->data, mmio->len);
    }

}

void mmio_handler(struct mmio_access *mmio) {
    if (
        mmio->phys_addr >= UART_16550_BASE &&
        mmio->phys_addr < UART_16550_BASE + 0x8
    ) {
        uart_handler(mmio);
    }
}

void set_vgic_attr(int fd, uint32_t group, uint32_t attr, uint64_t addr) {
    int rc;
    struct kvm_device_attr dev_attr = {
        .group = group,
        .attr = attr,
        .addr = (uint64_t)&addr,
    };
    rc = ioctl(fd, KVM_SET_DEVICE_ATTR, &dev_attr);
    if (rc == -1) {
        perror("KVM_SET_DEVICE_ATTR");
        exit(EXIT_FAILURE);
    }
}

void get_vgic_attr(int fd, uint32_t group, uint32_t attr, uint64_t addr) {
    int rc;
    struct kvm_device_attr dev_attr = {
        .group = group,
        .attr = attr,
        .addr = (uint64_t)&addr,
    };
    rc = ioctl(fd, KVM_GET_DEVICE_ATTR, &dev_attr);
    if (rc == -1) {
        perror("KVM_GET_DEVICE_ATTR");
        exit(EXIT_FAILURE);
    }
}

void configure_arm_vgic(int dev_fd) {
    set_vgic_attr(dev_fd, KVM_DEV_ARM_VGIC_GRP_ADDR, 
        KVM_VGIC_V2_ADDR_TYPE_DIST, VGIC_DIST_REGS_BASE);
    set_vgic_attr(dev_fd, KVM_DEV_ARM_VGIC_GRP_ADDR, 
        KVM_VGIC_V2_ADDR_TYPE_CPU, VGIC_CPU_REGS_BASE);
}

int main (int argc, char *argv[]) {
    int kvm_fd, rc;

    kvm_fd = open(KVM_FILE, O_RDWR);
    if (kvm_fd < 0) {
        perror("Error opening kvm file");
        return EXIT_FAILURE;
    }

    rc = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (rc < 0) {
        perror("KVM_GET_API_VERSION");
        return rc;
    }
    
    printf("KVM API VERSION: %d\n", rc);

    if (rc != 12) {
        exit(EXIT_FAILURE);

    }
    
    if (!ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_CHECK_EXTENSION_VM)) {
        printf("KVM_CAP_CHECK_EXTENSION_VM not available. Exiting...\n");
        exit(EXIT_FAILURE);
    }

    // Create VM
    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, KVM_VM_TYPE_ARM_IPA_SIZE(32));

    rc = ioctl(vm_fd, KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY);
    if (rc == -1) {
        perror("KVM_CHECK_EXTENSION");
        exit(EXIT_FAILURE);
    } else if (rc != 1) {
        printf("KVM_SET_USER_MEMORY capability not available. Exiting...\n");
        exit(EXIT_SUCCESS);
    }

    int guest_file = open("/usr/lib/guest.img", O_RDONLY);
    struct stat st;
    if (guest_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Allocate 12MB page aligned memory
    void *normal_mem = aligned_alloc(PAGE_SIZE, NORM_MEM_SIZE);
    if (normal_mem == NULL) {
        perror("aligned alloc");
        exit(EXIT_FAILURE);
    }
    memset(normal_mem, 0, NORM_MEM_SIZE);  
    fstat(guest_file, &st);
    rc = read(guest_file, normal_mem, st.st_size);
    if (rc < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // Allocate 4MB page aligned memory
    void *mmio_mem = aligned_alloc(PAGE_SIZE, MMIO_SIZE);
    if (mmio_mem == NULL) {
        perror("aligned alloc");
        exit(EXIT_FAILURE);
    }
    memset(mmio_mem, 0, MMIO_SIZE);

    struct kvm_userspace_memory_region normal_mem_region = {
        .slot = (uint32_t)0,                          // Slot number (this is like an index for memory regions assigned to a VM)
        .guest_phys_addr = (uint64_t)NORM_MEM_BASE,             // This memory will start at byte 0 in the guest
        .memory_size = (uint64_t)NORM_MEM_SIZE,      // Memory region size
        .userspace_addr = (uint64_t)normal_mem     // Pointer to userspace memory
    };

    // Assign previously allocated memory to VM
    rc = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &normal_mem_region);
    if (rc == -1) {
        perror("KVM_SET_USER_MEMORY");
        exit(EXIT_FAILURE);
    }

    struct kvm_userspace_memory_region mmio_mem_region = {
        .slot = (uint32_t)1,
        .flags = KVM_MEM_READONLY,
        .guest_phys_addr = (uint64_t)MMIO_BASE,
        .memory_size = (uint64_t)MMIO_SIZE,
        .userspace_addr = (uint64_t)mmio_mem
    };

    rc = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mmio_mem_region);
    if (rc == -1) {
        perror("KVM_SET_USER_MEMORY");
        exit(EXIT_FAILURE);
    }
    
    // Check maximum and recommended number of vCPUs for a VM
    int rec_vcpus, max_vcpus, max_vcpu_id;
    rec_vcpus = ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_VCPUS);
    if (rec_vcpus == 0) {
        rec_vcpus = 4;
    }
    max_vcpus = ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_MAX_VCPUS);
    if (max_vcpus == 0) {
        max_vcpus = rec_vcpus;
    }
    max_vcpu_id = ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_MAX_VCPU_ID);
    if (max_vcpu_id == 0) {
        max_vcpu_id = max_vcpus;
    }
    printf("Maximum number of VCPUs supported: %d\n", max_vcpus);
    printf("Recommended number of VCPUs: %d\n", rec_vcpus);

    // Create virtual CPU for VM 
    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd == -1) {
        perror("Error creating VCPU");
        exit(EXIT_FAILURE);
    }
    
    // Memory map vcpu fields into kvm_run structure
    int vcpu_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    printf("KVM VCPU MMAP SIZE: %d\n", vcpu_mmap_size);

    struct kvm_run *run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0); 
    if (run == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Create ARM vGIC (Generic Interrupt Controller)
    struct kvm_create_device arm_vgic = {
        .type = KVM_DEV_TYPE_ARM_VGIC_V2
    };
    rc = ioctl(vm_fd, KVM_CREATE_DEVICE, &arm_vgic);
    if (rc == -1) {
        perror("KVM_CREATE_DEVICE");
        exit(EXIT_FAILURE);
    }
    configure_arm_vgic(arm_vgic.fd);

    struct kvm_vcpu_init vcpu_init;
    rc = ioctl(vm_fd, KVM_ARM_PREFERRED_TARGET, &vcpu_init);
    if (rc == -1) {
        perror("KVM_ARM_PREFERRED_TARGET");
        exit(EXIT_FAILURE);
    }

    rc = ioctl(vcpu_fd, KVM_ARM_VCPU_INIT, &vcpu_init);
    if (rc == -1) {
        perror("KVM_ARM_VCPU_INIT");
        exit(EXIT_FAILURE);
    }

    while (1) {
        ioctl(vcpu_fd, KVM_RUN, NULL);
        //printf("Exit Reason: %d\n", run->exit_reason);

        switch(run->exit_reason) {
            case KVM_EXIT_SYSTEM_EVENT:
                printf("System event exit.\n");
                return EXIT_SUCCESS;
            case KVM_EXIT_MMIO:
                mmio_handler(&run->mmio);
                break;
            default:
                printf("Unknown exit reason\n");
        }
    }

    free(normal_mem);
    free(mmio_mem);
    free(run);

    return EXIT_SUCCESS;
}