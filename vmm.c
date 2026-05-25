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
#define PAGE_SIZE 0x1000    // define 4 KB page size

struct list_entry {
    int fd;
    SLIST_ENTRY(list_entry) entries;
};

struct vm {
    int fd;
    SLIST_HEAD(vcpu_list_head, list_entry);
    SLIST_HEAD(device_list_head, list_entry);
};

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
    int guest_mem_size;
    struct stat st;
    if (guest_file == -1) {
        perror("open");
        guest_mem_size = 0x1000000;
    } else {
        fstat(guest_file, &st);
        ssize_t file_size = st.st_size;
        guest_mem_size = file_size + (PAGE_SIZE - (file_size & (PAGE_SIZE - 1))); 
    }

    // Allocate page aligned memory
    void *mem = mmap(NULL, guest_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, guest_file, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    struct kvm_userspace_memory_region mem_region = {
        .slot = (uint32_t)0,                          // Slot number (this is like an index for memory regions assigned to a VM)
        .guest_phys_addr = (uint64_t)0x0,             // This memory will start at byte 0 in the guest
        .memory_size = (uint64_t)guest_mem_size,      // Memory region size
        .userspace_addr = (uint64_t)mem     // Pointer to userspace memory
    };

    // Assign previously allocated memory to VM
    rc = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mem_region);
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

    while (1) {
        ioctl(vcpu_fd, KVM_RUN, NULL);
        printf("Exit Reason: %d\n", run->exit_reason);

        switch(run->exit_reason) {
            case KVM_EXIT_SYSTEM_EVENT:
                printf("System event exit.\n");
                return EXIT_SUCCESS;
            case KVM_EXIT_UNKNOWN:
                printf("Unkown exit reason. Hardware exit reason number: %lu\n", run->hw.hardware_exit_reason);
                break;
            default:
                printf("Invalid exit reason\n");
                return EXIT_FAILURE;

        }
    }


    return EXIT_SUCCESS;
}