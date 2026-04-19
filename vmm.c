#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <unistd.h>

#define KVM_FILE "/dev/kvm"

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
        printf("Error opening kvm file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    rc = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (rc < 0) {
        printf("ioctl error: %s\n", strerror(errno));
        return rc;
    }

    printf("KVM API VERSION: %d\n", rc);

    if (!ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_CHECK_EXTENSION_VM)) {
        printf("KVM_CAP_CHECK_EXTENSION_VM not available, exiting...");
        return EXIT_FAILURE;
    }

    long long vcpu_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    printf("KVM VCPU MMAP SIZE: %lld\n", vcpu_mmap_size);

    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, KVM_VM_TYPE_ARM_IPA_SIZE(40));

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

    for (int i = 0; i < rec_vcpus; i++) {
        int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, i);
    }

    return EXIT_SUCCESS;
}