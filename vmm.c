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

#define MMIO_START 0x200000
#define MMIO_SIZE 0xc00000

#define UART_16550_START MMIO_START + 0x000000

// 16550 UART Register Map
#define RBR_16550 0x00   // Receiver Buffer Register
#define THR_16550 0x01   // Transmitter Holding Register
#define IER_16550 0x02   // Interrupt Enable Register
#define IIR_16550 0x03   // Interrupt Identification Register
#define FCR_16550 0x04   // FIFO Control Register
#define LCR_16550 0x05   // Line Control Register
#define MCR_16550 0x07   // MODEM Control Register
#define LSR_16550 0x08   // Line Status Register
#define MSR_16550 0x09   // MODEM Status Register
#define SCR_16550 0x0a   // Scratch Register
#define DLL_16550 0x0b   // Divisor Latch (LS)
#define DLM_16550 0x0c   // Divisor Latch (LM)

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
    printf("UART handler triggered\n");
    if (mmio->is_write) {
        // TODO: This prints out-of-order. Fix
        printf("UART string received: ");
        write(STDOUT_FILENO, (void*)mmio->data, mmio->len);
        printf("\n");
    }

}

void mmio_handler(struct mmio_access *mmio) {
    printf("MMIO handler triggered.\n");
    if (
        mmio->phys_addr >= UART_16550_START &&
        mmio->phys_addr < UART_16550_START + 0xc
    ) {
        uart_handler(mmio);
    }
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
    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, KVM_VM_TYPE_ARM_IPA_SIZE(40));

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

    // Allocate 4MB page aligned memory
    void *normal_mem = aligned_alloc(PAGE_SIZE, 0x200000);
    if (normal_mem == NULL) {
        perror("aligned alloc");
        exit(EXIT_FAILURE);
    }
    memset(normal_mem, 0, 0x200000);  
    fstat(guest_file, &st);
    rc = read(guest_file, normal_mem, st.st_size);
    if (rc < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // Allocate 12MB page aligned memory
    void *mmio_mem = aligned_alloc(PAGE_SIZE, 0xc00000);
    if (mmio_mem == NULL) {
        perror("aligned alloc");
        exit(EXIT_FAILURE);
    }
    memset(mmio_mem, 0, 0xc00000);

    struct kvm_userspace_memory_region normal_mem_region = {
        .slot = (uint32_t)0,                          // Slot number (this is like an index for memory regions assigned to a VM)
        .guest_phys_addr = (uint64_t)0x0,             // This memory will start at byte 0 in the guest
        .memory_size = (uint64_t)0x200000,      // Memory region size
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
        .guest_phys_addr = (uint64_t)0x200000,
        .memory_size = (uint64_t)0xc00000,
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
    
    int vcpu_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    printf("KVM VCPU MMAP SIZE: %d\n", vcpu_mmap_size);

    struct kvm_run *run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0); 
    if (run == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

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
        }
    }

    free(normal_mem);
    free(mmio_mem);
    free(run);

    return EXIT_SUCCESS;
}