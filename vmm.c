#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <linux/virtio_mmio.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <std.h>
#include <virtio.h>
#include <uart.h>

static virtio_blk_device blk_dev;

void init_virtio_blk_device(virtio_blk_device *blk_dev) {
    memset(blk_dev, 0, sizeof(virtio_blk_device));

    blk_dev->regs = (struct virtio_mmio_device_registers){
        .magic_number = 0x74726976,
        .version = 0x2,
        .device_id = 0x2,
        .vendor_id = 0x73696e61,   // LE for "anis"
        .queue_num_max = 0x8,
    };
    blk_dev->regs.device_features = VIRTIO_SUPPORTED_FEATURES;

    blk_dev->config.capacity = (1U << 30);
    blk_dev->config.blk_size = 4096;
}

void mmio_handler(struct mmio_access *mmio) {
    if (
        mmio->phys_addr >= UART_16550_BASE &&
        mmio->phys_addr < UART_16550_BASE + 0x8
    ) {
        uart_handler(mmio);
    } else if (
        mmio->phys_addr >= VIRTIO_BLK_BASE &&
        mmio->phys_addr < VIRTIO_BLK_BASE + PAGE_SIZE
    ) {
        virtio_handler(mmio, &blk_dev);
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
    // void *mmio_mem = aligned_alloc(PAGE_SIZE, MMIO_SIZE);
    // if (mmio_mem == NULL) {
    //     perror("aligned alloc");
    //     exit(EXIT_FAILURE);
    // }
    // memset(mmio_mem, 0, MMIO_SIZE);

    struct kvm_userspace_memory_region normal_mem_region = {
        // Slot number (this is like an index for memory regions assigned to a VM)
        .slot = (uint32_t)0,
        // This memory will start at byte 0 in the guest                          
        .guest_phys_addr = (uint64_t)NORM_MEM_BASE,
        // Memory region size             
        .memory_size = (uint64_t)NORM_MEM_SIZE,
        // Pointer to userspace memory      
        .userspace_addr = (uint64_t)normal_mem     
    };

    // Assign previously allocated memory to VM
    rc = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &normal_mem_region);
    if (rc == -1) {
        perror("KVM_SET_USER_MEMORY");
        exit(EXIT_FAILURE);
    }

    // struct kvm_userspace_memory_region mmio_mem_region = {
    //     .slot = (uint32_t)1,
    //     // Use readonly flag to make writes to this region cause KVM_EXIT_MMIO
    //     .flags = KVM_MEM_READONLY,
    //     .guest_phys_addr = (uint64_t)MMIO_BASE,
    //     .memory_size = (uint64_t)MMIO_SIZE,
    //     .userspace_addr = (uint64_t)mmio_mem
    // };

    // rc = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mmio_mem_region);
    // if (rc == -1) {
    //     perror("KVM_SET_USER_MEMORY");
    //     exit(EXIT_FAILURE);
    // }
    
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

    struct kvm_run *run = mmap(NULL, vcpu_mmap_size, 
                            PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0); 
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

    init_virtio_blk_device(&blk_dev);

    struct kvm_vcpu_init vcpu_init;
    memset(&vcpu_init, 0, sizeof(vcpu_init));
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

        switch(run->exit_reason) {
            case KVM_EXIT_SYSTEM_EVENT:
                printf("System event exit.\n");
                return EXIT_SUCCESS;
            case KVM_EXIT_MMIO:
                mmio_handler((struct mmio_access*)&run->mmio);
                break;
            default:
                printf("Unknown exit reason: %d\n", run->exit_reason);
        }
    }

    free(normal_mem);
    // free(mmio_mem);
    free(run);

    return EXIT_SUCCESS;
}