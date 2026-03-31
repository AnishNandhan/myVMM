# Overview

A minimal Type-2 hypervisor for ARM64 written in C. Drives the Linux KVM
subsystem directly to create and run a hardware-isolated virtual machine
on the Raspberry Pi 4. No QEMU, no libvirt, no existing VMM library, every
component is written from scratch against the raw KVM ioctl interface.
