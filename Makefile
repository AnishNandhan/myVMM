INCLUDES += ./include

all: vmm

vmm: vmm.o virtio.o uart.o
	$(CC) $(LDFLAGS) -o vmm vmm.o virtio.o uart.o

vmm.o: vmm.c
	$(CC) $(CFLAGS) -I$(INCLUDES) -c vmm.c

uart.o: uart.c
	$(CC) $(CFLAGS) -I$(INCLUDES) -c uart.c

virtio.o: virtio.c
	$(CC) $(CFLAGS) -I$(INCLUDES) -c virtio.c

.PHONY: clean
clean:
	rm -f *.o vmm