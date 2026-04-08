#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KVM_FILE "/dev/kvm"

int main (int argc, char *argv[]) {
    int kvm_fd, rc;

    kvm_fd = open(KVM_FILE, O_RDWR);
    if (kvm_fd < 0) {
        printf("Error opening kvm file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    rc = ioctl(kvm_fd, KVM_GET_API_VERSION);
    if (rc < 0) {
        printf("ioctl error: %s\n", strerror(errno));
        return rc;
    }

    printf("KVM API VERSION: %d\n", rc);

    return EXIT_SUCCESS;
}